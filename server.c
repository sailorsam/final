#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>


#define MAX_EVENTS 256

int set_nonblock(int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

ssize_t  sock_fd_write(int sock, void *buf, ssize_t buflen, int fd)
{
    ssize_t     size;
    struct msghdr   msg;
    struct iovec    iov;
    union {
        struct cmsghdr  cmsghdr;
        char        control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr  *cmsg;

    iov.iov_base = buf;
    iov.iov_len = buflen;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (fd != -1) {
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(sizeof (int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        //printf ("passing fd %d\n", fd);
        *((int *) CMSG_DATA(cmsg)) = fd;
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
       // printf ("not passing fd\n");
    }

    size = sendmsg(sock, &msg, 0);

    /*if (size < 0)
        perror ("sendmsg");*/
    return size;
}

ssize_t
sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
    ssize_t     size;

    if (fd) {
        struct msghdr   msg;
        struct iovec    iov;
        union {
            struct cmsghdr  cmsghdr;
            char        control[CMSG_SPACE(sizeof (int))];
        } cmsgu;
        struct cmsghdr  *cmsg;

        iov.iov_base = buf;
        iov.iov_len = bufsize;

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);
        size = recvmsg (sock, &msg, 0);
        if (size < 0) {
            //perror ("recvmsg");
            exit(1);
        }
        cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
            if (cmsg->cmsg_level != SOL_SOCKET) {
                //fprintf (stderr, "invalid cmsg_level %d\n",                     cmsg->cmsg_level);
                exit(1);
            }
            if (cmsg->cmsg_type != SCM_RIGHTS) {
                //fprintf (stderr, "invalid cmsg_type %d\n",                cmsg->cmsg_type);
                exit(1);
            }

            *fd = *((int *) CMSG_DATA(cmsg));
            //printf ("received fd %d\n", *fd);
        } else
            *fd = -1;
    } else {
        size = read (sock, buf, bufsize);
        if (size < 0) {
            //perror("read");
            exit(1);
        }
    }
    return size;
}


void worker(int sock, char * path)
{
    int fd;
    char    buf[16];
    ssize_t size;

    sleep(1);
    for (;;) {
        size = sock_fd_read(sock, buf, sizeof(buf), &fd);
        if (size <= 0)
            break;
        //printf ("read %d\n", (int) size);
        if (fd != -1) {
                const unsigned int BUF_SIZE = 1024;
                char buffer [BUF_SIZE];
                int res = recv(fd, buffer, BUF_SIZE, MSG_NOSIGNAL);
                if ((res == 0) && (errno != EAGAIN)) {

                }
                else if (res > 0) {
			FILE *fp;
			unsigned char flag = 0, version = 1;
			char * pt1, *pt2, *pt3, *pt4;
			char resp[4096]= {0};
			char url[64] = {0};
			time_t rtime;
			char h_error_v1[] = "HTTP/1.0 404 Not found\r\nContent-Type: text/html\r\nContent-Length:0\r\n\r\n";//<html><header>Not found!</header><body><h1>Not found!</h1></body></html>";
			char h_error_v0[] = "HTTP/0.9 404 Not Found\r\n\r\n";
			//char header[] = "HTTP/%d.%d 200 OK\nDate: %s\nServer: Test\nContent-type: text/html\nContent-Length:";	//"\n\n"	
			memcpy(resp,h_error_v1,sizeof(h_error_v1));
			time(&rtime);
	             if(buffer[0] == 'q')
				exit(0);	
			      
			/*if(strstr(buffer, "text/html"))
				flag = 1;*/
			
			pt3 = strstr(buffer, "HTTP/");
			if(pt3 == NULL)
				flag = 0;
			if(*(pt3 + 5) == '0')
				version = 0;
			if(version)
				sprintf(resp, "%s",h_error_v1);
			else
				sprintf(resp, "%s",h_error_v0);

			if(strstr(buffer, "GET") == NULL)
				flag = 0;

			pt1 = memchr(buffer, '/', BUF_SIZE);
			if(pt1 != NULL && flag == 1){
				pt2 = pt1 + strcspn(pt1, " ?");

				if(1){
					char full_path[96] = {0};
					char buf[2048] = {0}; 
					int len = 0;
					//printf("len %d\n", (int)(pt2-pt1));
					memcpy(url, pt1, pt2-pt1);
					sprintf(full_path, "%s%s",path, url);
					//printf("file path: %s\n", full_path);
					fp = fopen(full_path,"r");
					if(fp != NULL){
						len = fread(buf, 1, sizeof(buf), fp);
						if(version)
							sprintf(resp, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length:%d\r\n\r\n%s", /*ctime(&rtime),*/len,buf);
						else 	sprintf(resp, "HTTP/0.9 200 OK\r\nContent-Length:%d\r\n\r\n%s",len,buf);
						fclose(fp);
					}//fp
				}//pt2
			}//pt1
				
			 send(fd, resp, strlen(resp), MSG_NOSIGNAL);
		    }//res>0
                    shutdown(fd, SHUT_RDWR);
                    close(fd);
		     //printf("socket closed!\n");

                }
        }
 }


int main(int argc, char **argv)
{
char path[64] = "/home/box/final";
char  ip[16] = "127.0.0.1";
int port = 12345;
int opt;
int server_socket, conn_socket;
struct epoll_event events[MAX_EVENTS];
struct epoll_event event;
struct sockaddr_in sock_addr;
int epollfd, nfds, i, robin = 0;
int pid;
int on = 1;
int sv[2][2];

pid = fork();
if(pid != 0) return 0;

pid = fork();
if(pid != 0) return 0;

while ((opt = getopt(argc, argv, "h:p:d:")) != -1) {
               switch (opt) {
               case 'h':
			//ip.assign( optarg) ;
			sprintf(ip,"%s", optarg);
                  	 break;
               case 'p':
                   port = atoi(optarg);
                   break;
               case 'd':
                   //path.assign(optarg);
		   sprintf(path, "%s", optarg);
                   break;
               default: /* '?' */
                  //std::cout << "Usage: " << argv[0] << " [-h host_ip] [-p host_port] [-d directory]" << std::endl;
			printf("Ussage: %s [-h host_ip] [-p host_port][-d directory]",argv[0]);
                  return -1;
               }
           }


for(i = 0; i < 1 ; i++){
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sv[i]) < 0) {
       // printf("socketpair error");
        exit(1);
    }
    switch ((pid = fork())) {
    case 0:
        close(sv[i][0]);
        worker(sv[i][1], path);
        return 0;
    case -1:
        //printf("fork error");
        exit(1);
    default:
        close(sv[i][1]);
       /// parent(sv[i][0]);
        break;
    }
}

    server_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);   
    setsockopt ( server_socket,  SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = inet_addr(ip);
     //inet_pton(AF_INET, ip, &sock_addr);
   
    bind(server_socket, (struct sockaddr *) (&sock_addr), sizeof(sock_addr));
   
    set_nonblock(server_socket);
   
    listen(server_socket, SOMAXCONN);
   
    epollfd = epoll_create1(0);
      if (epollfd == -1) {
                            goto error;
           }

    event.data.fd = server_socket;
    event.events = EPOLLIN;
    if(epoll_ctl (epollfd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
               goto error;
           }
   
    while (1) {
        
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
	if(nfds == -1){
		goto error;
	}       

        for (i = 0; i < nfds; ++ i) {
            if (events[i].data.fd == server_socket) {
                conn_socket = accept(server_socket, 0, 0); 
                if (conn_socket == -1) {
                           goto error;
                       }
                
                set_nonblock(conn_socket);      
                    
                event.data.fd = conn_socket;
                event.events = EPOLLIN | EPOLLET;
                if(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_socket, & event) == -1){
		    goto error;
                }
               
            }
            else {
			epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
			if(robin ) robin = 0;
			else robin = 1;	
  			 sock_fd_write(sv[robin][0], "1", 1, events[i].data.fd);
			
            }
        }
}

error:
shutdown(server_socket, SHUT_RDWR);
close(server_socket); 
 
return 0;
}
