#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <errno.h>
#include <sys/shm.h>
#include <time.h>

#define PERM S_IRUSR | S_IWUSR //用户读写
#define MYPORT 3490   //定义通信端口号
#define BACKLOG 10  //定义服务器可以链接的最大客户端数量
#define WELCOME "|-------------------WELCOME TO THE CHAT ROOM!-----------------------|"  //党课胡连接上时显示的欢迎语句

//将int类型转换成char*类型
void iota(int i, char *string)
{
    int mask = 1;
    while (i / mask >= 10)
    {
        mask *= 10;
    }
    while (mask > 0)
    {
        *string++ = i /mask + '0';
        i %= mask;
        mask /= 10;
    }
    *string = '\0';
}

//得到当前时间
void get_cur_time(char *time_str)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    strcpy(time_str, ctime(&now.tv_sec));
}

//创建共享存储区
int shm_create()
{
    int shmid;
    if ((shmid = shmget(IPC_PRIVATE, 1024, PERM)) == -1)
    {
        fprintf(stderr, "Create share memory error:%s\n\a", strerror(errno));
    }
    return shmid;
}

//端口号绑定函数，创建套接字，并绑定到指定的端口号
int bindPort(unsigned short int port)
{
    int sockfd;
    struct sockaddr_in my_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);   //创建基于流的套接字
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;  //IPv4协议
    my_addr.sin_port = htons(port); //转换端口为网络字节序
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("fail to bind");
        exit(1);
    }

    printf("bind success!\n");
    return sockfd;
}

int main(int argc, char *argv[])
{
    int sockfd, clientfd;   //监听套接字，客户端套接字
    int sin_size, recvbytes;

    pid_t pid, ppid;  //定义父子进程标记
    char *buf, *read_addr, *write_addr, *temp, *time_str;  //需要用到的缓冲区
    struct sockaddr_in their_addr;  //定义地址结构
    int shmid;

    shmid = shm_create();  //创建共享存储区
    temp = (char *)malloc(255);\
    time_str = (char *)malloc(50);
    sockfd = bindPort(MYPORT);  //绑定端口号

    get_cur_time(time_str);
    printf("Time is : %s\n", time_str);

    if (listen(sockfd, BACKLOG) == -1)
    {
        //在指定的端口号上监听
        perror("fail to listen");
        exit(1);
    }

    printf("listen....\n");
    while (1)
    {
        //接受一个客户端的链接请求
        if ((clientfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1)
        {
            perror("fail to accept");
            exit(1);
        }

        //得到客户端的ip地址输出
        char address[20];
        inet_ntop(AF_INET, &their_addr.sin_addr, address, sizeof(address));

        printf("accept from %s\n", address);
        send(clientfd, WELCOME, strlen(WELCOME), 0);  //发送问候语
        buf = (char *)malloc(255);

        ppid = fork();  //创建子进程
        if (ppid == 0) //子进程
        {
            pid = fork(); //子进程创建子进程
            while (1)
            {
                if (pid > 0)
                {
                    memset(buf, 0, 255);
                    printf("OK\n");
                    if ((recvbytes = recv(clientfd, buf, 255, 0)) <= 0)
                    {
                        perror("fail to recv");
                        close(clientfd);
                        raise(SIGKILL);
                        exit(1);
                    }
                    write_addr = shmat(shmid, 0, 0);
                    memset(write_addr, '\0', 1024);

                    strncpy(write_addr, buf, 1024);
                    get_cur_time(time_str);
                    strcat(buf, time_str);
                    printf("%s\n", buf);
                } else if (pid == 0)
                {
                    sleep(1);
                    read_addr = shmat(shmid, 0, 0);
                    if (strcmp(temp, read_addr) != 0)
                    {
                        strcpy(temp, read_addr);
                        get_cur_time(time_str);
                        strcat(read_addr, time_str);
                        if (send(clientfd, read_addr, strlen(read_addr), 0) == -1)
                        {
                            perror("fail to send.");
                            exit(1);
                        }
                        memset(read_addr, '\0', 1024);
                        strcpy(read_addr, temp);
                    }
                } else  {
                    perror("fail to fork.");
                }

            }
        }
    }
    printf("----------------------------\n");
    free(buf);
    close(sockfd);
    close(clientfd);
    return 0;
}
