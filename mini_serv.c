#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>


typedef struct s_client
{
	int fd;
	int id;
	struct s_client *next;

} 	t_client;

t_client *clients = NULL;


int sockfd, connfd, len , g_id = 0;
fd_set sockets,  fd_read, fd_write;
char msg[3* 42 * 4096],  buff[3* 42 * 4096 +42];


void fatal()
{
	char str[] = "Fatal error\n";
	write(2,str, strlen(str));
	exit(1);
}

int get_id(int fd)
{
	int id;
	t_client *tmp = clients;

	while (tmp)
	{
		if (tmp->fd == fd)
			return tmp->id;
		tmp =  tmp->next;
	}



	return (-1);
}

void send_all(int fd)
{
	t_client *tmp = clients;
	while (tmp)
	{

		if(tmp->fd != fd && FD_ISSET(tmp->fd, &fd_write))
		{
			if(send(tmp->fd, buff,  strlen(buff), 0) < 0)
				fatal();
			bzero(&buff, sizeof(buff));
		
		}
		tmp = tmp->next;
	}
}


void extract_message(int fd)
{
	char	tmp[3 * 42 * 4096];
	int	i = 0, j = 0;

	bzero(tmp, sizeof(tmp));
	while (msg[i])
	{
		tmp[j++] = msg[i];
		if (msg[i] == '\n')
		{

			bzero(&buff, sizeof(buff));
			sprintf(buff, "client %d: %s",  get_id(fd), tmp);
			send_all(fd);
			j = 0;
			bzero(&tmp, sizeof(tmp));
			bzero(&buff, sizeof(buff));
		}
		i++;
	}
	bzero(&msg, sizeof(msg));
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int get_max_fd()
{
	t_client *tmp = clients;
	int max_fd = sockfd;
	while (tmp)
	{
		if (tmp->fd > max_fd)
		{
			max_fd = tmp->fd;
		}
		tmp = tmp->next;
	}
	return max_fd;
}

void add_client()
{

	struct sockaddr_in cli;
	int connfd;
	socklen_t len = sizeof(cli);
	t_client *new;
	connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	if (connfd < 0) { 
		fatal();
	}
	bzero(&buff, sizeof(buff));
	sprintf(buff, "server: client %d just arrived\n", g_id);
	FD_SET(connfd, &sockets);
	if (!(new = calloc(1, sizeof(*new))))
		fatal();
	new->fd = connfd;
	new->id = g_id++;
	new->next = NULL;
	if (!clients)
		clients = new;
	else
	{
		t_client *tmp = clients;
		while (tmp->next)
		{
			tmp = tmp->next;
		}
		tmp->next = new;
	}
	send_all(connfd);
}

void rm_client(int fd)
{
    t_client *dele = NULL;
    t_client *prev = NULL; // Pointer to keep track of the previous node
    sprintf(buff, "server: client %d just left\n", get_id(fd) );
    send_all(fd);

    // Check if the client to remove is at the head of the list
    if (clients && clients->fd == fd)
    {
        dele = clients;
        clients = clients->next; // Update the head
    }
    else
    {
        t_client *tmp = clients;
        while (tmp)
        {
            if (tmp->fd == fd)
            {
                dele = tmp;
                if (prev) // If there's a previous node
                {
                    prev->next = tmp->next; // Link the previous node to the next node
                }
                break; // Break out since we've found the client to remove
            }
            prev = tmp; // Move prev to current
            tmp = tmp->next; // Move to the next node
        }
    }

    // Free the deleted client node
    if (dele)
        free(dele);

    FD_CLR(fd, &sockets);
    close(fd);
}

//void rm_client(int fd)
//{
//	t_client *dele = NULL;
//	sprintf(buff, "server: client %d just left\n",get_id(fd) );
//	send_all(fd);
//	if(clients && clients->fd == fd)
//	{
//		dele = clients; 
//		clients = clients->next;
//	}
//	else {
//		t_client *tmp = clients;
//		while (tmp)
//		{
//			if (tmp->fd == fd)
//			{
//				dele = tmp;
//				tmp = tmp->next;
//				break;
//			}
//			tmp =  tmp->next;
//		}
//	}
//	if (dele)
//		free(dele);
//
//	FD_CLR(fd, &sockets);
//	close(fd);
//
//}
int main(int ac, char **av) {
	if (ac != 2)
	{
		write(2,  "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}
	struct sockaddr_in servaddr, cli; 
	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fatal();
	} 
	bzero(&servaddr, sizeof(servaddr)); 
	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		fatal();
	} 
	if (listen(sockfd, 10) != 0) {
		fatal();
	}
	FD_ZERO(&sockets);
	FD_SET(sockfd, &sockets);
	bzero(buff, sizeof(buff));
	bzero(msg,sizeof(msg) );
	while(1)
	{
		fd_read = fd_write = sockets;

		if(select(get_max_fd() +1, &fd_read, &fd_write, NULL, NULL) < 0)
			continue;
		for(int fd = sockfd; fd < get_max_fd() + 1; fd++)
		{
			if (FD_ISSET(fd, &fd_read))
			{

				if (fd  == sockfd)
				{

					add_client();
					break;
				}
				int ret_recv = 1;
				while (ret_recv == 1  && msg[strlen(msg) - 1] != '\n')
				{
					ret_recv = recv(fd, msg + strlen(msg), 1 , 0);
					if (ret_recv <= 0)
						break;
				}
				extract_message(fd);
				if(ret_recv <=0)
				{
					rm_client(fd);
					break;
				}
			}
		}
	}

	return 0;
}
