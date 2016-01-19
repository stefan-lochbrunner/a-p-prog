#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#if defined(__linux__)
#include <termios.h>
#else
#include <windows.h>
#endif


char* COM = "";
//char* COM = "/dev/ttyS0";

int com;

int baudRate;
int verbose = 1;
int verify = 1;
int program = 1;
int devid_expected;
int devid_mask;
unsigned char file_image[70000];
int flash_size = 4096;
int page_size = 32;
int sleep_time = 0;

void comErr(char *fmt, ...) {
	char buf[ 500 ];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	fprintf(stderr,"%s", buf);
	perror(COM);
	va_end(va);
	abort();
	}

void flsprintf(FILE* f, char *fmt, ...) {
	char buf[ 500 ];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	fprintf(f,"%s", buf);
	fflush(f);
	va_end(va);
	}


#if defined(__linux__)

void initSerialPort() {
	baudRate=B57600;
	if (verbose>2)
		printf("Opening: %s at %d\n",COM,baudRate);
	com =  open(COM, O_RDWR | O_NOCTTY | O_NDELAY);
	if (com <0)
		comErr("Failed to open serial port");

	struct termios opts;
	memset (&opts,0,sizeof (opts));

	fcntl(com, F_SETFL, 0);

	if (tcgetattr(com, &opts)!=0)
		{
		printf("Err tcgetattr\n");
		}

	cfsetispeed(&opts, baudRate);
	cfsetospeed(&opts, baudRate);

	opts.c_lflag  &=  ~(ICANON | ECHO | ECHOE | ISIG);

	opts.c_cflag |=  (CLOCAL | CREAD);
	opts.c_cflag &=  ~PARENB;
	opts.c_cflag &= ~CSTOPB;
	opts.c_cflag &=  ~CSIZE;
	opts.c_cflag |=  CS8;

	opts.c_oflag &=  ~OPOST;

	opts.c_iflag &=  ~INPCK;
	opts.c_iflag &=  ~(IXON | IXOFF | IXANY);
	opts.c_cc[ VMIN ] = 0;
	opts.c_cc[ VTIME ] = 10;//0.1 sec



	if (tcsetattr(com, TCSANOW, &opts) != 0) {
		perror(COM);
		printf("set attr error");
		abort();
		}

	tcflush(com,TCIOFLUSH); // just in case some crap is the buffers
	/*
	char buf = -2;
	while (read(com, &buf, 1)>0) {
		if (verbose)
			printf("Unexpected data from serial port: %02X\n",buf & 0xFF);
		}
*/
	}


	void putByte(int byte) {
	char buf = byte;
	if (verbose>3)
		flsprintf(stdout,"TX: 0x%02X\n", byte);
	int n = write(com, &buf, 1);
	if (n != 1)
		comErr("Serial port failed to send a byte, write returned %d\n", n);
	}

int getByte() {
	char buf;
	int n = read(com, &buf, 1);
	if (verbose>3)
		flsprintf(stdout,n<1?"RX: fail\n":"RX:  0x%02X\n", buf & 0xFF);
	if (n == 1)
		return buf & 0xFF;

	comErr("Serial port failed to receive a byte, read returned %d\n", n);
	return -1; // never reached
	}
#else

HANDLE port_handle;

void initSerialPort()
{

char mode[40],portname[20];
COMMTIMEOUTS timeout_sets;
DCB port_sets;
strcpy(portname,"\\\\.\\");
strcat(portname,COM);
  port_handle = CreateFileA(portname,
                      GENERIC_READ|GENERIC_WRITE,
                      0,                          /* no share  */
                      NULL,                       /* no security */
                      OPEN_EXISTING,
                      0,                          /* no threads */
                      NULL);                      /* no templates */
  if(port_handle==INVALID_HANDLE_VALUE)
  {
    printf("unable to open port %s -> %s\n",COM, portname);
    exit(0);
  }
  strcpy (mode,"baud=57600 data=8 parity=n stop=1");
  memset(&port_sets, 0, sizeof(port_sets));  /* clear the new struct  */
  port_sets.DCBlength = sizeof(port_sets);

  if(!BuildCommDCBA(mode, &port_sets))
  {
	printf("dcb settings failed\n");
	CloseHandle(port_handle);
	exit(0);
  }

  if(!SetCommState(port_handle, &port_sets))
  {
    printf("cfg settings failed\n");
    CloseHandle(port_handle);
    exit(0);
  }


  timeout_sets.ReadIntervalTimeout         = 1;
  timeout_sets.ReadTotalTimeoutMultiplier  = 100;
  timeout_sets.ReadTotalTimeoutConstant    = 1;
  timeout_sets.WriteTotalTimeoutMultiplier = 100;
  timeout_sets.WriteTotalTimeoutConstant   = 1;

  if(!SetCommTimeouts(port_handle, &timeout_sets))
  {
    printf("timeout settings failed\n");
    CloseHandle(port_handle);
    exit(0);
  }


}
void putByte(int byte)
{
  int n;
  	if (verbose>3)
		flsprintf(stdout,"TX: 0x%02X\n", byte);
  WriteFile(port_handle, &byte, 1, (LPDWORD)((void *)&n), NULL);
  	if (n != 1)
		comErr("Serial port failed to send a byte, write returned %d\n", n);
}

int getByte()
{
unsigned char buf[2];
int n;
ReadFile(port_handle, buf, 1, (LPDWORD)((void *)&n), NULL);
	if (verbose>3)
		flsprintf(stdout,n<1?"RX: fail\n":"RX:  0x%02X\n", buf[0] & 0xFF);
	if (n == 1)
		return buf[0] & 0xFF;
	comErr("Serial port failed to receive a byte, read returned %d\n", n);
	return -1; // never reached
	}
#endif



void sleep_ms (int num)
{
	struct timespec tspec;
	tspec.tv_sec=num/1000;
	tspec.tv_nsec=(num%1000)*1000000;
	nanosleep(&tspec,0);
}


/*
int getIntArg(char* arg) {
	if (strlen(arg)>=2 && memcmp(arg,"0x",2)==0) {
		unsigned int u;
		sscanf(arg+2,"%X",&u);
		return u;
		}
	else {
		int d;
		sscanf(arg,"%d",&d);
		return d;
		}
	}
	*/

void printHelp() {
		flsprintf(stdout,"pp programmer\n");
	exit(0);
	}


void setCPUtype(char* cpu) {
	if (strcmp("16f1454",cpu)==0)
		{
		flash_size = 16384;
		page_size = 64;
		devid_expected = 0x3020;
		devid_mask = 0x3FFF;
		}
	else if (strcmp("16f1503",cpu)==0)
		{
		flash_size = 4096;
		page_size = 32;
		devid_expected = 0x2CE0;
		devid_mask = 0xFFE0;
		}
	else if (strcmp("16f1507",cpu)==0) 
		{
		flash_size = 4096;		//bytes, where 1word = 2bytes, though actually being 14 bits
		page_size = 32;			//bytes
		devid_expected = 0x2D00;
		devid_mask = 0xFFE0;
		}
	else if (strcmp("16f1508",cpu)==0)
		{
		flash_size = 8192;
		page_size = 64;
		devid_expected = 0x2D20;
		devid_mask = 0xFFE0;
		}
	else if (strcmp("16f1509",cpu)==0)
		{
		flash_size = 16384;
		page_size = 64;
		devid_expected = 0x2D40;
		devid_mask = 0xFFE0;
		}
	else if (strcmp("16f1829",cpu)==0)
		{
		flash_size = 16384;
		page_size = 64;
		devid_expected = 0x27E0;
		devid_mask = 0xFFE0;
		}
	else if (strcmp("16lf1829",cpu)==0)
		{
		flash_size = 16384;
		page_size = 64;
		devid_expected = 0x28E0;
		devid_mask = 0xFFE0;
		}
	else if (strcmp("16f1825",cpu)==0)
		{
		flash_size = 16384;
		page_size = 64;
		devid_expected = 0x2760;
		devid_mask = 0xFFE0;
		}
	else if (strcmp("16lf1825",cpu)==0)
		{
		flash_size = 16384;
		page_size = 64;
		devid_expected = 0x2860;
		devid_mask = 0xFFE0;
		}

	else {
		flsprintf(stderr,"Unsupported CPU type '%s'\n",cpu);
		abort();
		}
	}

void parseArgs(int argc, char *argv[]) {
	int c;
	while ((c = getopt (argc, argv, "c:nps:t:v:")) != -1) {
		switch (c) {
			case 'c' :
				COM=optarg;
				break;
			case 'n':
		    	verify = 0;
			    break;
			case 'p':
		    	program = 0;
			    break;
			case 's' :
				sscanf(optarg,"%d",&sleep_time);
				break;
			case 't' :
				setCPUtype(optarg);
				break;
			case 'v' :
				sscanf(optarg,"%d",&verbose);
				break;
			case '?' :
				if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr,"Unknown option character `\\x%x'.\n",optopt);
			  default:
				fprintf (stderr,"Bug, unhandled option '%c'\n",c);
				abort ();
			}
		}
	if (argc<=1)
		printHelp();
	}



int enter_progmode (void)
	{
	if (verbose>2)
		flsprintf(stdout,"Entering programming mode\n");
	putByte(0x01);
	putByte(0x00);
	getByte();
	return 0;
	}

int exit_progmode (void)
	{
	if (verbose>2)
		flsprintf(stdout,"Exiting programming mode\n");

	putByte(0x02);
	putByte(0x00);
	getByte();
	return 0;
	}

int rst_pointer (void)
	{
	if (verbose>2)
		flsprintf(stdout,"Resetting PC\n");

	putByte(0x03);
	putByte(0x00);
	getByte();
	return 0;
	}


int mass_erase (void)
	{
	if (verbose>2)
		flsprintf(stdout,"Mass erase\n");

	putByte(0x07);
	putByte(0x00);
	getByte();
	return 0;
	}

int load_config (void)
	{
	if (verbose>2)
		flsprintf(stdout,"Load config\n");
	putByte(0x04);
	putByte(0x00);
	getByte();
	return 0;
	}

int inc_pointer (unsigned char num)
	{
	if (verbose>2)
		flsprintf(stdout,"Inc pointer %d\n",num);
	putByte(0x05);
	putByte(0x01);
	putByte(num);
	getByte();
	return 0;
	}


int program_page (unsigned int ptr, unsigned char num)
	{
	unsigned char i;
	if (verbose>2)
		flsprintf(stdout,"Programming page of %d bytes at %d\n", num,ptr);

	putByte(0x08);
	putByte(num+1);
	putByte(num);
	for (i=0;i<num;i++)
		putByte(file_image[ptr+i]);
	getByte();
	return 0;
	}

int read_page (unsigned char * data, unsigned char num)
{
unsigned char i;
	if (verbose>2)
		flsprintf(stdout,"Reading page of %d bytes\n", num);
putByte(0x06);
putByte(0x01);
putByte(num/2);
getByte();
for (i=0;i<num;i++)
	{
	*data++ = getByte();
	}
return 0;
}


int get_devid (void)
{
unsigned char tdat[20],devid_lo,devid_hi;
unsigned int retval;
rst_pointer();
load_config();
inc_pointer(6);
read_page(tdat, 4);
devid_hi = tdat[(2*0)+1];
devid_lo = tdat[(2*0)+0];
if (verbose>2) flsprintf(stdout,"Getting devid - lo:%2.2x,hi:%2.2x\n",devid_lo,devid_hi);
retval = (((unsigned int)(devid_lo))<<0) + (((unsigned int)(devid_hi))<<8);
return retval;
}

int get_config (unsigned char n)
{
unsigned char tdat[20],devid_lo,devid_hi;
unsigned int retval;
rst_pointer();
load_config();
inc_pointer(n);
read_page(tdat, 4);
devid_hi = tdat[(2*0)+1];
devid_lo = tdat[(2*0)+0];
if (verbose>2) flsprintf(stdout,"Getting config +%d - lo:%2.2x,hi:%2.2x\n",n,devid_lo,devid_hi);
retval = (((unsigned int)(devid_lo))<<0) + (((unsigned int)(devid_hi))<<8);
return retval;
}


int program_config(void)
{
rst_pointer();
load_config();
inc_pointer(7);
program_page(2*0x8007,2);
program_page(2*0x8008,2);
return 0;
}




int main(int argc, char *argv[])
	{
	unsigned char tdat[128];
	int i,j,devid,config,econfig;
	i = 5;

	printf ("\n");

	parseArgs(argc,argv);
	printf ("Opening serial port\n");
	initSerialPort();

	if (sleep_time>0)
		{
		printf ("Sleeping for %d ms\n", sleep_time);
		fflush(stdout);
		sleep_ms (sleep_time);
		}

	char* filename=argv[argc-1];
	if (verbose>2) printf ("Opening filename %s \n", filename);
	FILE* sf = fopen(filename, "rb");
	if (sf!=0)
		{
		if (verbose>2) printf ("File open\n");
		fread(file_image, 70000, 1, sf);
		fclose(sf);
		}
	else
		printf ("Error opening file \n");
	for (i=0;i<70000;i++)
		{
		if ((i%2)!=0)
			file_image[i] = 0x3F&file_image[i];
		}


	enter_progmode();

	devid = get_devid();
	if (devid_expected == (devid&devid_mask))
		printf ("Device ID 0x%4.4X\n",devid);
	else
		printf ("Unexpected device ID 0x%4.4X\n",devid);


	if (program==1)
		{
		mass_erase();
		rst_pointer();

		printf ("Programming FLASH (%d B in %d pages)",flash_size,flash_size/page_size);
		fflush(stdout);
		for (i=0;i<flash_size;i=i+page_size)
			{
			if (verbose>1)
				{
				printf (".");
				fflush(stdout);
				}
			program_page(i,page_size);
			}
		printf ("\n");
		printf ("Programming config\n");
		program_config();
		}
	if (verify==1)
		{
		printf ("Verifying FLASH (%d B in %d pages)",flash_size,flash_size/page_size);
		fflush(stdout);
		rst_pointer();
		for (i=0;i<flash_size;i=i+page_size)
			{
			if (verbose>1)
				{
				printf (".");
				fflush(stdout);
				}
			read_page(tdat,page_size);
			for (j=0;j<page_size;j++)
				{
				if (file_image[i+j] != tdat[j])
					{
					printf ("Error at 0x%4.4X E:0x%2.2X R:0x%2.2X\n",i+j,file_image[i+j],tdat[j]);
					exit_progmode();
					exit(0);
					}
				}
			}
		printf ("\n");
		printf ("Verifying config\n");
		config = get_config(7);
		econfig = (((unsigned int)(file_image[2*0x8007]))<<0) + (((unsigned int)(file_image[2*0x8007+1]))<<8);

		if (config==econfig)
			{
			if (verbose>1) printf ("config 1 OK: %4.4X\n",config);
			}
		else	printf ("config 1 error: E:0x%4.4X R:0x%4.4X\n",config,econfig);
		config = get_config(8);
		econfig = (((unsigned int)(file_image[2*0x8008]))<<0) + (((unsigned int)(file_image[2*0x8008+1]))<<8);
		if (config==econfig)
			{
			if (verbose>1) printf ("config 2 OK: %4.4X\n",config);
			}
		else	printf ("config 2 error: E:0x%4.4X R:0x%4.4X\n",config,econfig);
		}

	printf ("Releasing MCLR\n");
	exit_progmode();

	return 0;
	}
