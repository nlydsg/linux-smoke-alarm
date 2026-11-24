#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <strings.h>
#include <termios.h> //配置串口的头文件
#include <string.h>
#include <pthread.h>
#include <linux/input.h>

int x = -1,y = -1;
int zjm = 0;
int yz = 80;

int   uart_init(const char * uart_name)
{
	/*设置串口
	  波特率:9600
	  数据位:8
	  校验位:不要
	  停止位:1
	  数据流控制:无
	  */
	
	int   uart1_fd= open(uart_name, O_RDWR);//打开串口1设备文件
	if (uart1_fd == -1)
	{
		perror("open error:");
		return -1;
	}
	x = -1,y = -1;
	struct termios myserial;
	//清空结构体
	memset(&myserial, 0, sizeof (myserial));
	//O_RDWR               
	myserial.c_cflag |= (CLOCAL | CREAD);
	//设置控制模式状态，本地连接，接受使能
	//设置 数据位
	myserial.c_cflag &= ~CSIZE;   //清空数据位
	myserial.c_cflag &= ~CRTSCTS; //无硬件流控制
	myserial.c_cflag |= CS8;      //数据位:8

	myserial.c_cflag &= ~CSTOPB;//   //1位停止位
	myserial.c_cflag &= ~PARENB;  //不要校验

	cfsetospeed(&myserial, B9600);  //设置波特率,B9600是定义的宏
	cfsetispeed(&myserial, B9600);

	/* 刷新输出队列,清除正接受的数据 */
	tcflush(uart1_fd, TCIFLUSH);

	/* 改变配置 */
	tcsetattr(uart1_fd, TCSANOW, &myserial);
	return   uart1_fd;
}
unsigned char cmd[]={0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};

int value;
void get_value(int fd)
{
	int i=0,flag=0;
	unsigned char recvbuf[9]={0};
	write(fd,cmd,9);
	while(1)
	{
		unsigned char buf ;
		read(fd,&buf,1);
		if(buf == 0xFF)
		{
			flag = 1;
		}
		if(flag == 1)
		{
			recvbuf[i++] = buf;
			if(i == 9)
			{
				break;
			}
		}
	}
	value = recvbuf[2]<<8 | recvbuf[3];
}

void get_xy()
{
	int fd = open("/dev/input/event0",O_RDWR);
	if(fd == -1)
	{
		printf("open event0 faile\n");
		return ;
	}
	printf("open event0 ok\n");
	
	struct input_event ts;
	while(1)
	{
		//持续获取触摸屏的触摸数据
		read(fd,&ts,sizeof(struct input_event));
		//判断当前大事件类型是否为触摸屏类型
		if(ts.type == EV_ABS)
		{
			if(ts.code == ABS_X)
			{
				x = ts.value;
			}
			if(ts.code == ABS_Y)
			{
				y = ts.value;
			}
		}
		if(ts.type == EV_KEY && ts.code == BTN_TOUCH && ts.value == 0)
		{
			break;
		}
		printf("x = %d y = %d ts.type = %d ts.code = %d ts.value = %d\n",x,y,ts.type,ts.code,ts.value);
	}
	printf("x = %d y = %d\n",x,y);
	close(fd);
}

void show_bmp(char *bmpname,int x0,int y0)
{
	
	//打开驱动文件文件
	int fd = open("/dev/fb0",O_RDWR);
	//判断驱动文件是否打开成功
	if(fd == -1)		//fd < 0
	{
		printf("open fb0 faile\n");
		return ;
	}
	printf("open fb0 ok\n");
	//打开图片文件
	int bmp_fd = open(bmpname,O_RDWR);
	if(bmp_fd == -1)		//fd < 0
	{
		printf("open %s faile\n",bmpname);
		return ;
	}
	printf("open %s ok\n",bmpname);
	//利用图片前54字节特性获取图片的宽和高
	unsigned int w = 0,h = 0;
	lseek(bmp_fd,18,SEEK_SET);
	read(bmp_fd,(void *)&w,4);
	read(bmp_fd,(void *)&h,4);
	char bmp_color[w*h*3];
	bzero(bmp_color,w*h*3);
	lseek(bmp_fd,54,SEEK_SET);
	read(bmp_fd,bmp_color,w*h*3);
	char * shared = (char *)mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,
								MAP_SHARED,fd,0);
	//char buf1[800*480*4] = {0};
	//int buf2[800*480] = {0};
	for(int x = x0; x < w + x0;x++)
	{
		for(int y = y0;y < h + y0;y++)
		{
			if(x >= 0 && x < 800 && y >= 0 && y < 480)
			{
				//方法一	按照字节对齐的方式使用char类型存储
				shared[x*4+y*4*800+0] = bmp_color[(x - x0)*3+(h-(y-y0)-1)*3*w+0];
				shared[x*4+y*4*800+1] = bmp_color[(x - x0)*3+(h-(y-y0)-1)*3*w+1];
				shared[x*4+y*4*800+2] = bmp_color[(x - x0)*3+(h-(y-y0)-1)*3*w+2];
				shared[x*4+y*4*800+3] = 0;
				
				//方式二	按照in类型使用位运算的方式进行存储
				//shared[x+y*800] = bmp_color[x*3+y*3*800+0]<<24|bmp_color[x*3+y*3*800+1]<<16|bmp_color[x*3+y*3*800+2]<<8|0;
			}
		}
	}
	munmap(shared,800*480*4);
	close(bmp_fd);
	close(fd);
	
}

void cs(int s)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("open fb0");
        return ;
    }
    
    int width = 800;
    int height = 480;
    int screen_size = width * height * 4;
    
    unsigned int *plcd = (unsigned int *)mmap(NULL, 
                                              screen_size, 
                                              PROT_READ | PROT_WRITE, 
                                              MAP_SHARED, 
                                              fd, 
                                              0);
    
    if (plcd == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return ;
    }
    
    int color = s;  // 蓝色
    
    //指定颜色
    for (int i = 0; i < width * height; i++) {
        plcd[i] = color;
    }
    
    // 解除映射
    if (munmap(plcd, screen_size) == -1) {
        perror("munmap");
    }
    
    // 关闭文件描述符
    close(fd);
}

// 显示0-999数字的函数，50x50大小，固定显示3位数字（带前导零）
void show_number(int num, int color, int bg_color, int x0, int y0) {
    // 简单的8x10数字点阵（0-9），1表示显示像素
    static const unsigned char digit_bitmaps[10][10] = {
        // 0
        {0x3E, 0x7F, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x7F, 0x3E},
        // 1
        {0x0C, 0x1C, 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F},
        // 2
        {0x3E, 0x7F, 0x63, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x63, 0x7F},
        // 3
        {0x3E, 0x7F, 0x63, 0x03, 0x0E, 0x0E, 0x03, 0x63, 0x7F, 0x3E},
        // 4
        {0x06, 0x0E, 0x1E, 0x36, 0x66, 0x66, 0x7F, 0x06, 0x06, 0x06},
        // 5
        {0x7F, 0x7F, 0x60, 0x60, 0x7E, 0x03, 0x03, 0x63, 0x7F, 0x3E},
        // 6
        {0x3E, 0x7F, 0x63, 0x60, 0x7E, 0x73, 0x63, 0x63, 0x7F, 0x3E},
        // 7
        {0x7F, 0x7F, 0x03, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x18, 0x18},
        // 8
        {0x3E, 0x7F, 0x63, 0x63, 0x3E, 0x3E, 0x63, 0x63, 0x7F, 0x3E},
        // 9
        {0x3E, 0x7F, 0x63, 0x63, 0x7F, 0x3F, 0x03, 0x63, 0x7F, 0x3E}
    };
    
    // 限制数字范围0-999
    if (num < 0) num = 0;
    if (num > 999) num = 999;
    
    // 打开fb设备
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("open fb0");
        return;
    }
    
    int width = 800;
    int height = 480;
    int screen_size = width * height * 4;
    
    unsigned int *plcd = (unsigned int *)mmap(NULL, 
                                              screen_size, 
                                              PROT_READ | PROT_WRITE, 
                                              MAP_SHARED, 
                                              fd, 
                                              0);
    
    if (plcd == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }
    
    // 分离出百位、十位、个位（总是显示3位，带前导零）
    int digits[3];
    
    // 计算各个位数
    digits[0] = num / 100;           // 百位
    digits[1] = (num % 100) / 10;    // 十位
    digits[2] = num % 10;            // 个位
    
    // 固定背景尺寸 150x50
    int bg_width = 150;
    int bg_height = 50;
    
    // 确保背景区域不超出屏幕边界
    int bg_x_end = x0 + bg_width;
    int bg_y_end = y0 + bg_height;
    
    if (bg_x_end > width) bg_x_end = width;
    if (bg_y_end > height) bg_y_end = height;
    
    // 绘制150x50的背景区域
    for (int y = y0; y < bg_y_end && y < height; y++) {
        for (int x = x0; x < bg_x_end && x < width; x++) {
            if (x >= 0 && y >= 0) {
                plcd[x + y * width] = bg_color;
            }
        }
    }
    
    // 计算数字开始位置（在150x50区域内居中显示）
    // 数字总宽度：3 * 50 + 2 * 10 = 170，但背景只有150宽
    // 调整数字宽度和间距以适应150x50背景
    
    // 重新定义数字参数以适应150x50背景
    int digit_width = 40;     // 每个数字宽度40像素
    int digit_height = 40;    // 每个数字高度40像素
    int spacing = 5;          // 数字间隔5像素
    
    int total_digit_width = 3 * digit_width + 2 * spacing;  // 总共130像素
    int start_x = x0 + (bg_width - total_digit_width) / 2;  // 水平居中
    int start_y = y0 + (bg_height - digit_height) / 2;      // 垂直居中
    
    // 绘制3个数字（固定显示3位）
    for (int d = 0; d < 3; d++) {
        int digit = digits[d];
        int digit_start_x = start_x + d * (digit_width + spacing); // 每个数字位置
        
        for (int y = 0; y < digit_height; y++) {
            for (int x = 0; x < digit_width; x++) {
                // 从8x10缩放到digit_width x digit_height
                int src_x = x * 8 / digit_width;
                int src_y = y * 10 / digit_height;
                
                // 检查点阵中对应的位是否为1
                if (digit_bitmaps[digit][src_y] & (0x80 >> src_x)) {
                    int screen_x = digit_start_x + x;
                    int screen_y = start_y + y;
                    
                    // 检查边界
                    if (screen_x >= 0 && screen_x < width && 
                        screen_y >= 0 && screen_y < height) {
                        plcd[screen_x + screen_y * width] = color;
                    }
                }
            }
        }
    }
    
    // 清理
    munmap(plcd, screen_size);
    close(fd);
}


void *fun(void *arg)
{
	while(1)
	{
		get_xy();
		if(zjm == 1)
		{
			//主界面是否触发+-按键
			if(y > 380 && y < 470)
			{
				if(x > 240 && x < 380)  //+
				{
					yz++;
				} 
				else if(x > 630 && x < 770) //-
				{
					yz--;
				} 
			}
		}
	}
}
//拖动显示效果
void show_bmp_R_L(char *bmpname,int x0,int y0)
{
    
    //打开驱动文件文件
    int fd = open("/dev/fb0",O_RDWR);
    //判断驱动文件是否打开成功
    if(fd == -1)        //fd < 0
    {
        printf("open fb0 faile\n");
        return ;
    }
    printf("open fb0 ok\n");
    //打开图片文件
    int bmp_fd = open(bmpname,O_RDWR);
    if(bmp_fd == -1)        //fd < 0
    {
        printf("open %s faile\n",bmpname);
        return ;
    }
    printf("open %s ok\n",bmpname);
    //利用图片前54字节特性获取图片的宽和高
    unsigned int w = 0,h = 0;
    lseek(bmp_fd,18,SEEK_SET);
    read(bmp_fd,(void *)&w,4);
    read(bmp_fd,(void *)&h,4);
    char bmp_color[w*h*3];
    bzero(bmp_color,w*h*3);
    lseek(bmp_fd,54,SEEK_SET);
    read(bmp_fd,bmp_color,w*h*3);
    char * shared = (char *)mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,
                                MAP_SHARED,fd,0);
    int nub = h / 4 ;
    for(int z = x0+w-1;z>=x0;z-=8)
    {
        for(int x = z; x < w + x0;x++)
        {
            for(int y = y0;y < h + y0;y++)
            {
                if(x >= 0 && x < 800 && y >= 0 && y < 480 && x >= x0 && x < x0+w)
                {
                    shared[x*4+y*4*800+0] = bmp_color[(x - z)*3+(h-(y-y0)-1)*3*w+0];
                    shared[x*4+y*4*800+1] = bmp_color[(x - z)*3+(h-(y-y0)-1)*3*w+1];
                    shared[x*4+y*4*800+2] = bmp_color[(x - z)*3+(h-(y-y0)-1)*3*w+2];
                    shared[x*4+y*4*800+3] = 0;
                }
            }
        }
    }
    //释放空间
    munmap(shared,800*480*4);
    //关闭图片文件
    close(bmp_fd);
    //关闭驱动文件
    close(fd);
}

//利用传参的方式进行显示图片
int main()
{
	pthread_t pid;
	pthread_create(&pid,NULL,fun,NULL);
	int uart1_fd = uart_init("/dev/ttySAC1");
    cs(0xffffff);
    show_bmp("jiesuo.bmp",0,0);
    while (1)
    {	
		if(zjm == 0 && x > 360 && x < 630 && y > 260 && y < 370)
		{		
			//解锁动画	
			show_bmp_R_L("draw.bmp",0,0);
			zjm = 1;
			show_bmp("draw.bmp",0,0);
		}
		if(zjm == 1)
		{
			//显示实际值与阈值			
			get_value(uart1_fd);
			printf("MQ2 value:%d\n",value);
			show_number(value,0x6495ED,0x000000,295,107);
			show_number(yz,0x6495ED,0x000000,295,165);
			if(value > yz)
			{
				show_bmp("jb.bmp",0,400);
			}
			else
			{
				show_bmp("cb.bmp",0,400);
			}
			sleep(1);
		}
		
    }
    
	close(uart1_fd);
    return 0;
}