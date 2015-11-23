#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<time.h>

#define MAXFNAME	14
#define BLKSIZE		512
#define CACHE       10
#define TOTBLK      8196
#define DBLKSIZE    8186
#define SPLBLK      10
#define MAXDIR      (BLKSIZE/sizeof(struct DirEntry))
char *devfiles[] = {"DEV0",NULL};
int devfd[] = {-1, -1};
int curDir=9;
// Data structure definitions
struct SupBlock {
	char sb_vname[MAXFNAME];										//Superblock name
	int	sb_nino;													//no. of inodes
	int	sb_nblk;													//no. of blocks
	int	sb_nrootdir;												//pointer to root dir
	int	sb_nfreeblk;												//no. of free blocks
	int	sb_nfreeino;												//no. of free inodes
	int	sb_flags;													//device flag
	unsigned short sb_freeblks[CACHE];                              //free blocks list
	unsigned short sb_freeinos[CACHE];		                        //free inode list
	int	sb_freeblkindex;											//pointer to block
	int	sb_freeinoindex;											//pointer to inode
	unsigned int	sb_chktime;
	unsigned int	sb_ctime;

};
struct SupBlock sb;

struct OnDiskDirEntry {
	char	d_name[MAXFNAME];
	unsigned short	d_ino;
};

struct DirEntry {
	struct OnDiskDirEntry d_entry;
	int	d_offset;
};

#define ROOTDIRSIZE	((4 * 512)/sizeof(struct OnDiskDirEntry))
struct INode {
	unsigned int	i_size;											//file size
	unsigned int	i_atime;										//access time
	unsigned int	i_ctime;										//creation time
	unsigned int	i_mtime;										//modified time
	unsigned int	i_blks[13];										//pointer to data
	short		i_mode;												//file modes
	unsigned int	i_uid;											//file uid
	unsigned int	i_gid;											//file gid
	unsigned int	i_gen;
	unsigned int	i_lnk;
};

#define INODETABSIZE	((8 * 512) / sizeof(struct INode))

struct InCoreINode {
	struct INode 	ic_inode;
	int 		ic_ref;
	int		ic_ino;
	short		ic_dev;
	struct InCoreINode	*ic_next;
	struct InCoreINode	*ic_prev;
};


struct OpenFileObject {
	struct InCoreINode	*ofo_inode;
	int			ofo_mode;
	unsigned int		ofo_curpos;
	int			ofo_ref;
};
struct INode nullinode;
//============= TESTING APPLICATION USING THESE FS CALLS ==============

// Menu driven testing application for creation of fs,
// and all file and directory related operations
int main()
{
    OpenDevice(0);
    MkFS(0);
    MkDir(0,"Rushabh",1,2,0);
    MkDir(0,"Folder",1,2,0);
    MkDir(0,"Gaurav",1,2,0);
    MkDir(0,"Sumit",1,2,0);
    MkDir(0,"Shishir",1,2,0);
    RmDir(0,curDir,"Folder");
    ShutdownDevice(0);
}

//============= SYSTEM CALL LEVEL NOT FOLLOWED =======

//============= VNODE/VFS NOT FOLLOWED ===============

//============== UFS INTERFACE LAYER ==========================
int MkFS(int dev)
{


    //make superblock
    char buf[512];
    bzero(&buf,BLKSIZE);
    write(devfd[dev],&buf,BLKSIZE);;
	strcpy(sb.sb_vname,"root");
	sb.sb_nino=INODETABSIZE;
	printf("%d\n",sb.sb_nino);
	sb.sb_nrootdir=9;
	sb.sb_nblk=8196-10;
	sb.sb_nfreeblk=8196-10;
	sb.sb_nfreeino=INODETABSIZE;
	sb.sb_flags=0;
	bzero(&sb.sb_freeblks,CACHE);
	bzero(&sb.sb_freeinos,CACHE);
    sb.sb_freeblkindex=CACHE;
    sb.sb_freeinoindex=CACHE;
    sb.sb_chktime=time(NULL);
    sb.sb_ctime=sb.sb_chktime;
    printf("%u\n",time(NULL));
    write(devfd[dev],&sb,sizeof(struct SupBlock));
    bzero(&buf,BLKSIZE-sizeof(struct SupBlock));
    write(devfd[dev],&buf,BLKSIZE-sizeof(struct SupBlock));
    printf("Super Block Created\n");


/**********************/
    //Init inode table

    nullinode.i_atime=0;
    memset(nullinode.i_blks,0,sizeof(nullinode.i_blks));
    nullinode.i_ctime=0;
    nullinode.i_gen=0;
    nullinode.i_gid=0;
    nullinode.i_lnk=0;
    nullinode.i_mode=0;
    nullinode.i_mtime=0;
    nullinode.i_size=0;
    nullinode.i_uid=0;
    int i=0;
    for(i=0;i<INODETABSIZE;i++)
    {
        write(devfd[dev],&nullinode,sizeof(struct INode));
    }
    bzero(&buf,BLKSIZE%sizeof(struct INode));
    write(devfd[dev],&buf,BLKSIZE%sizeof(struct INode));
    printf("Inode table created\n");


/************************************/
    //Fill the data block
    bzero(&buf,BLKSIZE);
    for(i=0;i<8196-10;i++)
        write(devfd[dev],&buf,BLKSIZE);
    printf("Data block Initialised\n");

    fetchFreeBlocks(dev);
    fetchFreeInodes(dev);
    makeRootDir(dev);


    /***********************************/
	// Write initialized list of inodes

	// Write initialized list of directory entries

	// Fill the remaining empty datablocks

	// Write free blk information (data structures)
}

int makeRootDir(int dev)
{
    struct INode tmpinode;
    tmpinode.i_atime=tmpinode.i_ctime=tmpinode.i_mtime=time(NULL);
    tmpinode.i_blks[0]=AllocBlk(dev);
    tmpinode.i_lnk=0;
    tmpinode.i_gen=0;
    tmpinode.i_gid=tmpinode.i_uid=1;
    tmpinode.i_mode=0;
    struct DirEntry dir;
    dir.d_entry.d_ino=AllocInode(dev);
    strcpy(dir.d_entry.d_name,".");
    dir.d_offset=0;
    WriteDir(dev,&tmpinode,&dir);
    tmpinode.i_size=BLKSIZE;
    WriteInode(dev,dir.d_entry.d_ino,&tmpinode);
}

int fetchFreeBlocks(int dev)
{
    int i=0,j=0;
    char buf[512];
    lseek(devfd[dev],BLKSIZE*SPLBLK,SEEK_SET);
    for(j=SPLBLK+1;j<TOTBLK;j++)
    {
        read(devfd[dev],&buf,BLKSIZE);
        if(buf[0]==0)
        {
            sb.sb_freeblks[i++]=j;
            //printf("%d\n",i);
            if(i==CACHE)
            break;
        }
    }
    if(i==0)
    return 0;
    else
    return 1;
}
int fetchFreeInodes(int dev)
{
    int i=0,j=0;
    char buf[512];
    struct INode tmpinode;
    lseek(devfd[dev],BLKSIZE*2,SEEK_SET);
    for(j=0;j<INODETABSIZE;j++)
    {
        read(devfd[dev],&tmpinode,sizeof(tmpinode));
        if(tmpinode.i_size==0)
        {
            sb.sb_freeinos[i++]=j;
            printf("%d\n",sb.sb_freeinos[i-1]);
            if(i==CACHE)
            break;
        }
    }
    if(i==0)
    return 0;
    else
    return 1;
}
/*****************************************************************
// Open/create a file in the given directory with specified uid, gid
// and attributes. Type of modes are limited to read, write and append only.
int OpenFile(int dirhandle, char *fname, int mode, int uid, int gid, int attrib)
{

}

// Close a file
int CloseFile(int fhandle)
{
}

// Read from a file already opened, nbytes into buf
int ReadFile(int fhandle, char buf[], int nbytes)
{
}

// Write into a file already opened, nbytes from buf
int WriteFile(int fhandle, char buf[], int nbytes)
{
}

// Move the file pointer to required position
int SeekFile(int fhandle, int pos, int whence)
{
}
************************************************/
// Create a directory
int MkDir(int dev, char *dname, int uid, int gid, int attrib)
{
    struct INode tmpinode;
    ReadInode(dev,curDir,&tmpinode);
    printf("link %lu\n",tmpinode.i_lnk);
    struct DirEntry dir;
    strcpy(dir.d_entry.d_name,dname);
    printf("%s\n",dir.d_entry.d_name);
    dir.d_entry.d_ino=AllocInode(dev);
    dir.d_offset=0;
    if((tmpinode.i_lnk+1)%MAXDIR==0)
    tmpinode.i_blks[(tmpinode.i_lnk+1)/MAXDIR]=AllocBlk(dev);
    printf("INO :%d\n",dir.d_entry.d_ino);
    tmpinode.i_atime=time(NULL);
    WriteDir(dev,&tmpinode,&dir);
    tmpinode.i_size=(tmpinode.i_lnk/BLKSIZE)+BLKSIZE;
    WriteInode(dev,curDir,&tmpinode);
    tmpinode.i_atime=tmpinode.i_ctime=tmpinode.i_mtime=time(NULL);
    tmpinode.i_blks[0]=AllocBlk(dev);
    tmpinode.i_lnk=0;
    tmpinode.i_gen=0;
    tmpinode.i_gid=gid;
    tmpinode.i_uid=uid;
    tmpinode.i_mode=0;
    bzero(dir.d_entry.d_name,MAXFNAME);
    strcpy(dir.d_entry.d_name,".");
    WriteDir(dev,&tmpinode,&dir);
    strcpy(dir.d_entry.d_name,"..");
    WriteDir(dev,&tmpinode,&dir);
    tmpinode.i_size=BLKSIZE;
    printf("INO :%d\n",dir.d_entry.d_ino);
    WriteInode(dev,dir.d_entry.d_ino,&tmpinode);
    return 1;
}

int fileExist(int dev,struct INode * ind,char *fname)
{
    int i,j;
    struct DirEntry dir;
    printf("Links %d\n",ind->i_lnk);
    for(i=0;i<(ind->i_lnk-1)/MAXDIR;i++)
    {
        lseek(devfd[dev],ind->i_blks[i]*BLKSIZE,SEEK_SET);
        for(j=0;j<MAXDIR;j++)
        {
            read(devfd[dev],&dir,sizeof(struct DirEntry));
            printf("%s\n",dir.d_entry.d_name);
            if(strcmp(dir.d_entry.d_name,fname)==0)
            {
                int r=dir.d_entry.d_ino;
                printf("END pos %d\n",ind->i_blks[(ind->i_lnk-1)/MAXDIR]*BLKSIZE+(ind->i_lnk-1)%MAXDIR*sizeof(struct DirEntry));
                lseek(devfd[dev],ind->i_blks[(ind->i_lnk-1)/MAXDIR]*BLKSIZE+(ind->i_lnk-1)%MAXDIR*sizeof(struct DirEntry),SEEK_SET);
                read(devfd[dev],&dir,sizeof(struct DirEntry));
                printf("CUR pos %d\n",ind->i_blks[i]*BLKSIZE+j*sizeof(struct DirEntry));
                lseek(devfd[dev],ind->i_blks[i]*BLKSIZE+j*sizeof(struct DirEntry),SEEK_SET);
                write(devfd[dev],&dir,sizeof(struct DirEntry));
                lseek(devfd[dev],ind->i_blks[(ind->i_lnk-1)/MAXDIR]*BLKSIZE+(ind->i_lnk-1)%MAXDIR*sizeof(struct DirEntry),SEEK_SET);
                dir.d_offset=0;
                dir.d_entry.d_ino=0;
                bzero(dir.d_entry.d_name,MAXFNAME);
                write(devfd[dev],&dir,sizeof(struct DirEntry));
                ind->i_mtime=ind->i_atime=time(NULL);
                ind->i_lnk--;
                if((ind->i_lnk-1)%MAXDIR==MAXDIR-1)
                {
                    ind->i_size-=BLKSIZE;
                    FreeBlk(dev,ind->i_blks[((ind->i_lnk-1)+1)%MAXDIR]);
                }
                WriteInode(dev,curDir,ind);
                return r;
            }
        }
    }
    lseek(devfd[dev],ind->i_blks[i]*BLKSIZE,SEEK_SET);
    for(j=0;j<(ind->i_lnk-1)%MAXDIR;j++)
    {
        read(devfd[dev],&dir,sizeof(struct DirEntry));
        printf("%s\n",dir.d_entry.d_name);
        if(strcmp(dir.d_entry.d_name,fname)==0)
            {
                int r=dir.d_entry.d_ino;
                printf("END pos %d\n",ind->i_blks[(ind->i_lnk-1)/MAXDIR]*BLKSIZE+(ind->i_lnk-1)%MAXDIR*sizeof(struct DirEntry));
                lseek(devfd[dev],ind->i_blks[(ind->i_lnk-1)/MAXDIR]*BLKSIZE+(ind->i_lnk-1)%MAXDIR*sizeof(struct DirEntry),SEEK_SET);
                read(devfd[dev],&dir,sizeof(struct DirEntry));
                printf("CUR pos %d\n",ind->i_blks[i]*BLKSIZE+j*sizeof(struct DirEntry));
                lseek(devfd[dev],ind->i_blks[i]*BLKSIZE+j*sizeof(struct DirEntry),SEEK_SET);
                write(devfd[dev],&dir,sizeof(struct DirEntry));
                lseek(devfd[dev],ind->i_blks[(ind->i_lnk-1)/MAXDIR]*BLKSIZE+(ind->i_lnk-1)%MAXDIR*sizeof(struct DirEntry),SEEK_SET);
                dir.d_offset=0;
                dir.d_entry.d_ino=0;
                bzero(dir.d_entry.d_name,MAXFNAME);
                write(devfd[dev],&dir,sizeof(struct DirEntry));
                ind->i_mtime=ind->i_atime=time(NULL);
                ind->i_lnk--;
                if((ind->i_lnk-1)%MAXDIR==MAXDIR-1)
                {
                    ind->i_size-=BLKSIZE;
                    FreeBlk(dev,ind->i_blks[(ind->i_lnk)/MAXDIR]);
                }
                WriteInode(dev,curDir,ind);
                return r;
            }
    }
    return -1;
}
// Delete directory (if itis empty)
int RmDir(int dev,int ino, char *dname)
{
    struct INode tmpinode;
    printf("INODE %d\n",ino);
    ReadInode(dev,ino,&tmpinode);
    int d;
    struct DirEntry dir;
    if((d=fileExist(dev,&tmpinode,dname))>=0)
    {
        printf("EXISTS %d\n",d);
        int j,i;
        ReadInode(dev,d,&tmpinode);
        printf("Links %u",tmpinode.i_lnk);
        for(i=0;i<tmpinode.i_lnk/MAXDIR;i++)
        {
            lseek(devfd[dev],tmpinode.i_blks[i],SEEK_SET);
            for(j=0;j<MAXDIR;j++)
            {
                read(devfd[dev],&dir,sizeof(struct DirEntry));
                printf("%s\n",dir.d_entry.d_name);
                if(!(strcmp(dir.d_entry.d_name,"..")==0||strcmp(dir.d_entry.d_name,".")==0))
                DelDir(dev,dir.d_entry.d_ino);
            }
        }
        lseek(devfd[dev],tmpinode.i_blks[i]*BLKSIZE,SEEK_SET);
        for(j=0;j<tmpinode.i_lnk%MAXDIR;j++)
        {
            read(devfd[dev],&dir,sizeof(struct DirEntry));
            printf("%s\n",dir.d_entry.d_name);
            if(!(strcmp(dir.d_entry.d_name,"..")==0||strcmp(dir.d_entry.d_name,".")==0))
                DelDir(dev,dir.d_entry.d_ino);
        }
        printf("BLOCKS %u",tmpinode.i_size);
        for(i=0;i<tmpinode.i_size/BLKSIZE;i++)
        {
            printf("Block %u\n",tmpinode.i_blks[i]);
            FreeBlk(dev,tmpinode.i_blks[i]);
        }
        FreeInode(dev,ino);
    }

}

int DelDir(int dev,int ino)
{
    int j,i;
    printf("Called deldir\n");
    struct INode tmpinode;
    struct DirEntry dir;
    ReadInode(dev,i,&tmpinode);
    if(tmpinode.i_lnk>0)
    {
        for(i=0;i<tmpinode.i_lnk/MAXDIR;i++)
        {
            lseek(devfd[dev],tmpinode.i_blks[i],SEEK_SET);
            for(j=0;j<MAXDIR;j++)
            {
                read(devfd[dev],&dir,sizeof(struct DirEntry));
                if(!(strcmp(dir.d_entry.d_name,"..")==0||strcmp(dir.d_entry.d_name,".")))
                DelDir(dev,dir.d_entry.d_ino);
            }
        }
        lseek(devfd[dev],tmpinode.i_blks[i]*BLKSIZE,SEEK_SET);
        for(j=0;j<tmpinode.i_lnk%MAXDIR;j++)
        {
            read(devfd[dev],&dir,sizeof(struct DirEntry));
            if(!(strcmp(dir.d_entry.d_name,"..")==0||strcmp(dir.d_entry.d_name,".")))
                DelDir(dev,dir.d_entry.d_ino);
        }
    }
    for(i=0;i<tmpinode.i_size/BLKSIZE;i++)
    {
        FreeBlk(dev,tmpinode.i_blks[i]);
    }
    FreeInode(dev,ino);
}

int OpenDir(int pardir, char *dname)
{

}
/************************************************
int CloseDir(int dirhandle)
{
}

int SeekDir(int dirhandle, int pos, int whence)
{
}

int ReadDir(int dirhandle, struct DirEntry *dent)
{
}

**********************************************/
int WriteDir(int dev,struct INode *ind, struct DirEntry *dent)
{
    int pos=(ind->i_lnk)%MAXDIR;
    printf("%lu->>>>>%d\n",ind->i_blks[(ind->i_lnk)/MAXDIR],pos);
    lseek(devfd[dev],ind->i_blks[(ind->i_lnk)/MAXDIR]*BLKSIZE+(pos)*sizeof(struct DirEntry),SEEK_SET);
    printf("%lu\n",ind->i_blks[(ind->i_lnk)/MAXDIR]*BLKSIZE+(pos)*sizeof(struct DirEntry));
    printf("%s\n",dent->d_entry.d_name);
    write(devfd[dev],dent,sizeof(struct DirEntry));
    ind->i_lnk++;
}
/************************************************/
//============== UFS INTERNAL LOW LEVEL ALGORITHMS =============
int ReadInode(int dev, int ino, struct INode *inode)
{
    lseek(devfd[dev],BLKSIZE*2+ino*sizeof(struct INode),SEEK_SET);
    printf("Inode %d %lu\n",ino,BLKSIZE*2+ino*sizeof(struct INode));
    read(devfd[dev],inode,sizeof(struct INode));
}
/******************************************************/
int WriteInode(int dev, int ino, struct INode *inode)
{
    printf("%d\n",ino);
    lseek(devfd[dev],BLKSIZE*2+ino*sizeof(struct INode),SEEK_SET);
    printf("%lu\n",BLKSIZE*2+ino*sizeof(struct INode));
    write(devfd[dev],inode,sizeof(struct INode));
}
/********************************************************/
int AllocInode(int dev)
{
    if(sb.sb_freeinoindex==0)
    {
        int f=fetchFreeInodes(dev);
        if(f==0)
        return -1;
        sb.sb_freeinoindex=CACHE;
    }
    sb.sb_nfreeino--;
    printf("Free inode index :%d\n",sb.sb_freeinos[sb.sb_freeinoindex-1]);
    return sb.sb_freeinos[--sb.sb_freeinoindex];
}
int FreeInode(int dev, int ino)
{
    lseek(devfd[dev],BLKSIZE*2+ino*sizeof(struct INode),SEEK_SET);
    //printf("%lu",BLKSIZE*2+ino*sizeof(struct INode));
    write(devfd[dev],&nullinode,sizeof(struct INode));
}

int AllocBlk(int dev)
{
    if(sb.sb_freeblkindex==0)
    {
        int f=fetchFreeBlocks(dev);
        if(f==0)
        return -1;
        sb.sb_freeblkindex=CACHE;
    }
    sb.sb_nfreeblk--;
    return sb.sb_freeblks[--sb.sb_freeblkindex];
}

int FreeBlk(int dev, int blk)
{
    char buf[BLKSIZE];
    printf("Free block %d %d\n",blk,blk*BLKSIZE);
    memset(&buf,0,BLKSIZE);
    lseek(devfd[dev], blk * BLKSIZE, SEEK_SET);
	return write(devfd[dev], &buf, BLKSIZE);
}

//============== DEVICE DRIVER LEVEL =====================

// Reading a logical block blk from device dev
int ReadBlock(int dev, int blk, int buf[BLKSIZE])
{
	// Check for validity of the block
	// Check for validity of the device

	// If OK read the block
	lseek(devfd[dev], blk * BLKSIZE, SEEK_SET);
	return read(devfd[dev], buf, BLKSIZE);
}
/**************************************************
// Writing a logical block blk to device dev
int WriteBlock(int dev, int blk)
{
	// Check for validity of the block
	// Check for validity of the device

	// If OK write the block
	lseek(devfd[dev], blk * BLKSIZE, SEEK_SET);
	return write(devfd[dev], buf, BLKSIZE);
}
*****************************************************/

// Open the device
int OpenDevice(int dev)
{
	// Open the device related file for both reading and writing.
	//
	if ((devfd[dev] = open(devfiles[dev], O_RDWR|O_CREAT,0666)) < 0)
	{
		perror("Opening device file failure:");
		exit(0);
	}
	return devfd[dev];
}

// Shutdown the device
int ShutdownDevice(int dev)
{
	// if (dev < ... && dev > ...)
	if (devfd[dev] >= 0)
		close(devfd[dev]);

	return 0;
}

