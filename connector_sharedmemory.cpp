#include "connector_sharedmemory.h"





ipc::ipc_sharedmemory::ipc_sharedmemory(IPC_MODE connector_mode,const std::string connector_name,QObject* parent)
    :QObject(parent)
    ,pub_register_timer(new QTimer(parent)) 
    ,pub_publish_timer(new QTimer(parent))
    ,sub_recive_timer(new QTimer(parent))
{
    this->connector_name=connector_name;
    this->connector_mode=connector_mode;
    std::cout<<"connector_mode is:"<<(int)this->connector_mode<<std::endl;
    std::cout<<"connector_name is:"<<this->connector_name<<std::endl;

}

/*
ipc::ipc_sharedmemory::ipc_sharedmemory(const ipc_sharedmemory & ipc)
{
    connector_name=ipc.get_connector_name();
    connector_mode=ipc.get_connector_mode();
}
*/
/*
void ipc::ipc_sharedmemory::timerEvent(QTimerEvent *event)
{
    pub_register();
}
*/

std::string ipc::ipc_sharedmemory::get_connector_name()
{
    return this->connector_name;
}
ipc::IPC_MODE ipc::ipc_sharedmemory::get_connector_mode()
{
    return this->connector_mode;
}
QSharedMemory* ipc::ipc_sharedmemory::getSharedMemory()
{
    return ipc_memory_;
}

void ipc::ipc_sharedmemory::check_mode(IPC_MODE ipc_mode)
{
    if(this->connector_mode!=ipc_mode)
    {
        std::cout<<"<Wrong connector mode>"<<std::endl;
    }
}
void ipc::ipc_sharedmemory::check_port(std::string connector_name)
{

    if(ipc_memory_->key().toStdString().compare(connector_name)!=0)
    {
        std::stringstream ss;
        ss<<"Wrong connector name";
         std::cout<<ss.str();
    }
}

void ipc::ipc_sharedmemory::init(const std::string& inin_str)
{
    ipc_memory_=new QSharedMemory;
    switch(this->connector_mode)
    {
        case IPC_MODE::MODE_REQUEST:
        ipc_memory_->setKey(QString::fromStdString(connector_name));
        break;

        case IPC_MODE::MODE_RESPOND:
        ipc_memory_->setKey(QString::fromStdString(connector_name));
        break;

        case IPC_MODE::MODE_SUBSCRIBE:
        ipc_memory_->setKey(QString::fromStdString(connector_name));
        connect(sub_recive_timer,SIGNAL(timeout()),this,SLOT(sub_recive()));
        break;

        case IPC_MODE::MODE_PUBLISH:
        ipc_memory_->setKey(QString::fromStdString(connector_name));
        connect(pub_register_timer,SIGNAL(timeout()),this,SLOT(pub_register()));
        connect(pub_publish_timer,SIGNAL(timeout()),this,SLOT(pub_publish_test()));
        pub_register_timer->start(100);
        pub_publish_timer->start(10);
        break;
    }
    IPC_DATA*defaule_data_= new IPC_DATA;
    ipc::write_memory(ipc_memory_,defaule_data_);
}


bool ipc::ipc_sharedmemory::sub_register(std::string sub_name_)
{

    check_mode(IPC_MODE::MODE_SUBSCRIBE);
    IPC_DATA* register_data_=new IPC_DATA;
    register_data_->setValue(IPC_MODE::MODE_SUBSCRIBE,COMMAND_MODE::COMMAND_REGISTER,QString::fromStdString(sub_name_));
    while(true)
    {
        std::cout<<"Send register command..."<<std::endl;
        if(ipc::send_message(ipc_memory_,register_data_,COMMAND_MODE::COMMAND_DEFAULT))
        {
            break;
        }
        else if(ipc::send_message(ipc_memory_,register_data_,COMMAND_MODE::COMMAND_REG_FINSHED))
        {
            break;
        }
        QThread::sleep(1);
    }
    while(true)
    {
        std::cout<<"Wait for permmit..."<<std::endl;
        if(ipc::recive_message(ipc_memory_,register_data_,COMMAND_MODE::COMMAND_REG_FINSHED))
        {
            std::cout<<"Register as: "<<register_data_->data.toStdString()<<std::endl;
            ipc_sub_memory_=new QSharedMemory;
            ipc_sub_memory_->setKey(QString::fromStdString(sub_name_));
            IPC_DATA* default_data_=new IPC_DATA;
            ipc::write_memory(ipc_sub_memory_,default_data_);
            std::cout<<"Start to recive from pub server"<<std::endl;
            sub_recive_timer->start(10);
            break;
        }
        else if(ipc::recive_message(ipc_memory_,register_data_,COMMAND_MODE::COMMAND_REG_FAILED))
        {
            std::cout<<"Register faild: "<<sub_name_<<"is already exist!"<<std::endl;
            return false;
        }
    }

    return true;
}
//
//use Timer to drive pub_register pub_write
//
void ipc::ipc_sharedmemory::pub_register()
{
    //std::cout<<"Check register!"<<std::endl;

    check_mode(IPC_MODE::MODE_PUBLISH);
    IPC_DATA* register_data_=new IPC_DATA;
    //ipc::read_memory(ipc_memory_,register_data_);
    //register_data_->print();
    std::cout<<".";
    if(ipc::recive_message(ipc_memory_,register_data_,COMMAND_MODE::COMMAND_REGISTER))
    {
        std::cout<<std::endl<<"recive register command: "<<std::endl;
        QString sub_register_name_=register_data_->data;
        //register_data_->print();
        //std::cout<<sub_register_name_.toStdString()<<std::endl;
        if(pub_server_status_.contains(sub_register_name_))
        {
            register_data_->command_mode_=COMMAND_MODE::COMMAND_REG_FAILED;
            ipc::send_message(ipc_memory_,register_data_,COMMAND_MODE::COMMAND_REGISTER);
            //register_data_->print();
            std::cout<<"Name: "<<sub_register_name_.toStdString()<<" is already exist, try again!"<<std::endl;
            //return false;
        }
        else
        {
            ipc_sub_memory_=new QSharedMemory;
            ipc_sub_memory_->setKey(sub_register_name_);
            bool message_recived_=false;

            pub_server_status_[sub_register_name_]=qMakePair(ipc_sub_memory_,message_recived_);
            register_data_->command_mode_=COMMAND_MODE::COMMAND_REG_FINSHED;
            ipc::send_message(ipc_memory_,register_data_,COMMAND_MODE::COMMAND_REGISTER);
            std::cout<<"Register a sub_client as: "<<sub_register_name_.toStdString()<<std::endl;
        }
    }

    //return true;
}

std::string ipc::ipc_sharedmemory::sub_recive()
{
    IPC_DATA* sub_recive_data;
    sub_recive_data=new IPC_DATA;
    std::string recive_data_;
    //std::cout<<".";
    if(ipc::recive_message(ipc_sub_memory_,sub_recive_data,COMMAND_MODE::COMMAND_RESPONSE))
    {
        recive_data_=sub_recive_data->data.toStdString();
        std::cout<<"Recive data: "<<recive_data_<<std::endl;
        while(true)
        {
            sub_recive_data->command_mode_=COMMAND_MODE::COMMAND_RECIVE;
            if(ipc::send_message(ipc_sub_memory_,sub_recive_data,COMMAND_MODE::COMMAND_RESPONSE))
            {
                break;
            }
        }
    }


    delete sub_recive_data;
    return recive_data_;
}

void ipc::ipc_sharedmemory::pub_publish_test()
{
    if(!check_pub_status()){}
    else
    {
        std::cout<<"Sub clients [prepared!]"<<std::endl;
        IPC_DATA* publish_data_=new IPC_DATA;
        static int test=0;
        publish_data_->data=QString::number(test++,10);
        publish_data_->command_mode_=COMMAND_MODE::COMMAND_RESPONSE;

        QMapIterator <QString,QPair<QSharedMemory*,bool>> it_pub_status_(pub_server_status_);
        while(it_pub_status_.hasNext())
        {
            it_pub_status_.next();
            std::cout<<"["<<it_pub_status_.key().toStdString()<<"]";
            //QSharedMemory* sub_memory_;
            //sub_memory_=new QSharedMemory(it_pub_status_.key());

            ipc_sub_memory_->setKey(it_pub_status_.key());
            ipc::write_memory(ipc_sub_memory_,publish_data_);
            ipc::read_memory(ipc_sub_memory_,publish_data_);

        }
        delete publish_data_;
    }

}
bool ipc::ipc_sharedmemory::pub_publish(std::string data)
{
    if(!check_pub_status()){return false;}
    else
    {
        std::cout<<"Sub clients [prepared!]"<<std::endl;
        IPC_DATA* publish_data_=new IPC_DATA;
        publish_data_->data=QString::fromStdString(data);
        publish_data_->command_mode_=COMMAND_MODE::COMMAND_RESPONSE;

        QMapIterator <QString,QPair<QSharedMemory*,bool>> it_pub_status_(pub_server_status_);
        while(it_pub_status_.hasNext())
        {
            it_pub_status_.next();
            while(true)
            {
                if(ipc::send_message(it_pub_status_.value().first,publish_data_,COMMAND_MODE::COMMAND_RECIVE))
                {
                    break;
                }
                if(ipc::send_message(it_pub_status_.value().first,publish_data_,COMMAND_MODE::COMMAND_DEFAULT))
                {
                    break;
                }
            }

        }
        delete publish_data_;
    }
    return true;
}


bool ipc::ipc_sharedmemory::check_pub_status()
{
    if(pub_server_status_.size()==0)
    {
        return false;
    }

    QMapIterator <QString,QPair<QSharedMemory*,bool>> it_pub_status_(pub_server_status_);
    IPC_DATA* check_data=new IPC_DATA;
    size_t  num_prepare_=0;
    while(it_pub_status_.hasNext())
    {
        it_pub_status_.next();

        if(ipc::check_command(it_pub_status_.value().first,check_data,COMMAND_MODE::COMMAND_DEFAULT))
        {num_prepare_++;}
        else if(ipc::check_command(it_pub_status_.value().first,check_data,COMMAND_MODE::COMMAND_RECIVE))
        {num_prepare_++;}
        else
        {
            std::cout<<"Some sub client are not prepared!"<<std::endl;
            return false;
        }
    }

    delete check_data;
    return true;
}

std::string ipc::ipc_sharedmemory::req(const std::string& request)
{
    check_mode(IPC_MODE::MODE_REQUEST);
    IPC_DATA* req_data_=new IPC_DATA;
    req_data_->setValue(IPC_MODE::MODE_REQUEST,COMMAND_MODE::COMMAND_REQUEST,QString::fromStdString(request));

    while(true)
    {

        if(ipc::send_message(ipc_memory_,req_data_,COMMAND_MODE::COMMAND_DEFAULT))
        {
               break;

        }
        else if(ipc::send_message(ipc_memory_,req_data_,COMMAND_MODE::COMMAND_RECIVE))
        {
               break;
        }
    }
    //std::cout<<"Send request [Success!]"<<std::endl;
    while(true)
    {

        if(ipc::recive_message(ipc_memory_,req_data_,COMMAND_MODE::COMMAND_RESPONSE))
        {

            while(true)
            {
                req_data_->command_mode_=COMMAND_MODE::COMMAND_DEFAULT;
                if(ipc::send_message(ipc_memory_,req_data_,COMMAND_MODE::COMMAND_RESPONSE))
                {
                       break;
                }
            }
            break;
        }
    }



    std::string recive_data_=req_data_->data.toStdString();

    delete req_data_;
    return recive_data_;
}
void ipc::ipc_sharedmemory::rep_write(const std::string &respond)
{
    check_mode(IPC_MODE::MODE_RESPOND);
    IPC_DATA* rep_data_=new IPC_DATA;
    rep_data_->setValue(IPC_MODE::MODE_RESPOND,COMMAND_MODE::COMMAND_RESPONSE,QString::fromStdString(respond));

    while(true)
    {
        if(ipc::send_message(ipc_memory_,rep_data_,COMMAND_MODE::COMMAND_REQUEST))
        {
            break;
        }
        //QThread::sleep(SR_MESSAGE_DELAY);
    }
    delete rep_data_;
}
std::string ipc::ipc_sharedmemory::rep_read()
{
    check_mode(IPC_MODE::MODE_RESPOND);
    IPC_DATA* rep_data_=new IPC_DATA;
    rep_data_->setValue(IPC_MODE::MODE_RESPOND,COMMAND_MODE::COMMAND_RESPONSE,QString::fromStdString(""));

    while(true)
    {
        if(ipc::recive_message(ipc_memory_,rep_data_,COMMAND_MODE::COMMAND_REQUEST))
        {
            break;
        }
    }
    std::string recive_data_=rep_data_->data.toStdString();
    delete rep_data_;
    return recive_data_;
}




    //
    //
    //  Use the sharedMemory to write/read shared memory
    //  Only care about write/read data, dont care MQ mode
    //
    //
inline bool ipc::write_memory(QSharedMemory* qSharedMemory,IPC_DATA* ipc_data_)
{
    if(!qSharedMemory->isAttached())
    {
        qSharedMemory->attach();
    }

    //==================write to buffer=================//
    QBuffer buffer_temp_;
    buffer_temp_.open(QBuffer::ReadWrite);
    QDataStream write(&buffer_temp_);

    QMap <QString,QString>mempry_map_;
    mempry_map_["IPC_MODE"]=QString::number((int)ipc_data_->ipc_mode_,10);
    mempry_map_["COMMAND_MODE"]=QString::number((int)ipc_data_->command_mode_);
    mempry_map_["DATA"]=ipc_data_->data;
    write<<mempry_map_;


    int data_size=buffer_temp_.size();

    if(data_size==0)
    {
        std::stringstream ss;
        ss<<"Cant load data,check the message";
        std::cout<<ss.str();
        return false;
    }

    //============write to shared memory===============//
    if(qSharedMemory->size()==0)
    {
        if(!qSharedMemory->create(data_size))
        {
            //qSharedMemory->destroyed();
            std::cout<<"Cant create sharedMemory"<<std::endl;
        }
    }
    qSharedMemory->lock();
    char *to = static_cast<char *>(qSharedMemory->data());
    const char *from = buffer_temp_.data().data();
    memcpy(to, from, qMin((int)qSharedMemory->size(),(int)buffer_temp_.size()));
    qSharedMemory->unlock();

    return true;
}

inline bool ipc::read_memory(QSharedMemory* qSharedMemory,IPC_DATA* ipc_data_)
{

    if(!qSharedMemory->isAttached())
    {
        qSharedMemory->attach();
    }

    //=============read the memory=====================//
    QBuffer buffer_temp_;
    QDataStream read(&buffer_temp_);

    qSharedMemory->lock();
    buffer_temp_.setData(static_cast<const char *>(qSharedMemory->constData()), qSharedMemory->size());
    buffer_temp_.open(QBuffer::ReadOnly);

    QMap<QString,QString> memory_map_;
    read>>memory_map_;
    ipc_data_->ipc_mode_=(IPC_MODE)memory_map_["IPC_MODE"].toInt();
    ipc_data_->command_mode_=(COMMAND_MODE)memory_map_["COMMAND_MODE"].toInt();
    ipc_data_->data=memory_map_["DATA"];

    qSharedMemory->unlock();
    //qSharedMemory->detach();

    return true;
}
    //
    //
    //  Send/recive message using sharedMemory
    //  Take care of block in write/read method
    //
    //
inline bool ipc::check_command(QSharedMemory* qSharedMemory,IPC_DATA* ipc_check_data_,COMMAND_MODE commamd_mode_)
{
    while(true)
    {
        if(ipc::read_memory(qSharedMemory,ipc_check_data_))
        {
            break;
        }
    }
    if(ipc_check_data_->command_mode_!=commamd_mode_/*||(int)ipc_check_data_->command_mode_==0*/)
    {
        return false;
    }
    return true;
}
    //
    // when the command_mode_ fits,write ipc_data to memory;
    //
inline bool ipc::send_message(QSharedMemory* qSharedMemory,IPC_DATA* ipc_data_,COMMAND_MODE commamd_mode_)
{
    IPC_DATA* ipc_check_data_=new IPC_DATA;

    if(!ipc::check_command(qSharedMemory,ipc_check_data_,commamd_mode_))
    {
        return false;
    }

    if(!ipc::write_memory(qSharedMemory,ipc_data_))
    {
       return false;
    }
    return true;
}

    //
    //When the command_mode_ fits, read from memory to ipc_data_
    //
inline bool ipc::recive_message(QSharedMemory* qSharedMemory,IPC_DATA* ipc_data_,COMMAND_MODE commamd_mode_)
{
    IPC_DATA* ipc_check_data_=new IPC_DATA;
    IPC_DATA* ipc_default_data=new IPC_DATA;
    if(!ipc::check_command(qSharedMemory,ipc_check_data_,commamd_mode_))
    {
        return false;
    }
    ipc_data_->setValue(ipc_check_data_->ipc_mode_,ipc_check_data_->command_mode_,ipc_check_data_->data);
    return true;
}

























