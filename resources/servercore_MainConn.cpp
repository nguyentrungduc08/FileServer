/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include "../header/servercore.h"

void            
servercore::build_Select_list_For_Main_Connection()
{
    FD_ZERO(&(this->_mainConnSet));
    this->_highestFdMainSet = 0;
    std::vector<Connection*>::iterator _iter = this->_mainConnections.begin();

    while (_iter != this->_mainConnections.end()){
        if ( (*_iter)->get_Close_Request_Status() ){
            std::cout << "@log servercore: For_Main_Connection Connection with Id " << (*_iter)->getConnectionId() << " closed! " << std::endl;
            delete (*_iter);
            this->_mainConnections.erase(_iter);
            if ( this->_mainConnections.empty() || _iter == this->_mainConnections.end()){
                return;
            }
        } else {
            int _currentFD = (*_iter)->getFD();
            if (_currentFD != 0) {
                FD_SET(_currentFD, &(this->_mainConnSet)); 
                if (_currentFD > this->_highestFdMainSet)
                    this->_highestFdMainSet = _currentFD; 
            }
            ++_iter;
        }
    }
    return;
}

void            
servercore::read_Data_Main_Connections()
{
    for (unsigned int _index = 0; _index < this->_mainConnections.size(); ++_index) {
        if (FD_ISSET(this->_mainConnections.at(_index)->getFD(), &(this->_mainConnSet))) {
            std::cout << "@log servercore: read_Data_Main_Connections " << this->_mainConnections.at(_index)->getFD() << std::endl;
            if ( this->_mainConnections.at(_index)->get_isMainConnection() ){
                std::cout << "@log servercore: handle main connection" << std::endl;
                this->handle_Main_Connection(this->_mainConnections.at(_index));
                continue;
            }
        }
    }
}

void            
servercore::thread_Main_Connecion_Handle()
{
    int                 _num_Fd_Incomming;
    struct timeval      _time;
        
    while (!this->_shutdown) {
        std::cout << "@log servercore: Main thread waiting connections form client....." << std::endl;
        this->build_Select_list_For_Main_Connection();

        _time                 = this->_serverTimeout;
        _num_Fd_Incomming     = select(this->_highestFdMainSet+1, &(this->_mainConnSet), NULL, NULL, &_time);

        if (_num_Fd_Incomming < 0){
            std::cerr << "@log servercore: Error calling select()" << std::endl;
            return;
        }

        this->read_Data_Main_Connections();
    }    
    return;
}  

void            
servercore::update_List_Users_Active_Online(std::string _usernameOfConnection)
{
    rep(_index,this->_listUser.size()){
        if (this->_listUser.at(_index).username == _usernameOfConnection ){
            this->_listUser.at(_index)._state = true;
            return;
        }
    }
}

void 
servercore::update_List_Users_Active_Offline(std::string _usernameOfConnection)
{
    rep(_index,this->_listUser.size()){
        if (this->_listUser.at(_index).username == _usernameOfConnection ){
            this->_listUser.at(_index)._state = false;
            return;
        }
    }
}

void 
servercore::handle_Main_Connection(Connection* & _conn)
{
    Session*        _sessionOfConnection;
    unsigned int    _idOfConnection;
    TOKEN           _token;
    std::string     _usernameOfConnection;
    int             _idFileTransaction;
    int             _cmd;
    if ( !_conn->get_authen_state() ) {
        /* if not authen connection 
         * @TODO:
         * 1. authen login this connection.
         * 2. handle data comming
         * -if success:
         *      + update list user active
         *      + update list token
         * -if don't success:
         *      + set connection close    
         */
        if ( _conn->handle_CMD_AUTHEN_LOGIN(this->_listUser) ) {
            //if check auth success
            _conn->set_authen_state(true);
            _conn->respond_CMD_AUTHEN();
            _idOfConnection         = _conn->get_Connection_Id();
            _sessionOfConnection    = _conn->get_Session();
            _usernameOfConnection   = _conn->get_Username_Of_Connection();
            _token                  = std::make_pair(_idOfConnection,_sessionOfConnection);
            this->_listSession.pb(_token);
            this->update_List_Users_Active_Offline(_usernameOfConnection);
            std::cout << "@log servercore: add token " << this->_listSession.size() << " - " << _sessionOfConnection->getSession() << std::endl;
        } else {  
            _conn->set_Close_Request_Status(true); //close connection after response success login
        }
    } else {
        /*if this connection authenticated -> handle data commining
         * @TODO:
         * - handle PING - PONG keep alive main connection
         * - hanele send file transaction when exist file send to this user.
         */
        _usernameOfConnection   = _conn->get_Username_Of_Connection();
        
        std::cout << "@log servercore: main connection establish - num user active: " << this->get_Num_User_Active() << std::endl;
        _cmd = _conn->get_CMD_HEADER();
        
        switch (_cmd){
            case PING: 
                std::cout <<"@log servercore: PING packet from user " << _usernameOfConnection << std::endl;
                _conn->respond_PONG();
                break;
            case CMD_MSG_FILE:
                std::cout <<"@log servercore: CMD_MSG_FILE packet from user " << _usernameOfConnection << std::endl;
                break;
        }
        
    }
}