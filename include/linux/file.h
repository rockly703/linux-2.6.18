/*
 * Wrapper functions for accessing the file_struct fd array.
 */

#ifndef __LINUX_FILE_H
#define __LINUX_FILE_H

#include <asm/atomic.h>
#include <linux/posix_types.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/types.h>

/*
 * The default fd array needs to be at least BITS_PER_LONG,
 * as this is the granularity returned by copy_fdset().
 */
#define NR_OPEN_DEFAULT BITS_PER_LONG

/*
 * The embedded_fd_set is a small fd_set,
 * suitable for most tasks (which open <= BITS_PER_LONG files)
 */
struct embedded_fd_set {
	unsigned long fds_bits[1];
};

/*
 * More than this number of fds: we use a separately allocated fd_set
 */
#define EMBEDDED_FD_SET_SIZE (BITS_PER_BYTE * sizeof(struct embedded_fd_set))

struct fdtable {
	//当前文件对象的最大数, 通常max_fds比max_fdset小
	unsigned int max_fds;
	//文件描述符的最大数,在git bbea9f 中被干掉了
	int max_fdset;
	//管理一个打开文件的所有信息,指向files_struct的fd_array,数组长度由max_fds决定 
	struct file ** fd;      /* current fd array */
	//指向位域的指针, 该位域保存了所有在exec系统调用时需要关闭的文件描述符信息
	fd_set *close_on_exec;
	//open_fds指向的bit位最大数目由max_fdset决定,指向位域的指针,该位域保存所有打开文件的描述符
	//每个可能的文件描述符都对应一个比特位,如果比特位为1, 表示文件正在使用中.否则未使用
	fd_set *open_fds;
	struct rcu_head rcu;
	struct files_struct *free_files;
	struct fdtable *next;
};

/*
 * Open file table structure
 */
struct files_struct {
  /*
   * read mostly part
   */
   //引用计数
	atomic_t count;
	struct fdtable *fdt;
	struct fdtable fdtab;
  /*
   * written part on a separate cache line in SMP
   */
	spinlock_t file_lock ____cacheline_aligned_in_smp;
   //下一个可用的fd  
	int next_fd;
	//embedded_fd_set是位图,close_on_exec_init记录执行exec需要关闭的文件描述符
	struct embedded_fd_set close_on_exec_init;
	//最初的文件描述符集
	struct embedded_fd_set open_fds_init;
	//操作文件的数组, BITS_PER_LONG在32位机上是32, 在64位机上是64, 这和close_on_exec_init对应
	struct file * fd_array[NR_OPEN_DEFAULT];
};

#define files_fdtable(files) (rcu_dereference((files)->fdt))

extern void FASTCALL(__fput(struct file *));
extern void FASTCALL(fput(struct file *));

static inline void fput_light(struct file *file, int fput_needed)
{
	if (unlikely(fput_needed))
		fput(file);
}

extern struct file * FASTCALL(fget(unsigned int fd));
extern struct file * FASTCALL(fget_light(unsigned int fd, int *fput_needed));
extern void FASTCALL(set_close_on_exec(unsigned int fd, int flag));
extern void put_filp(struct file *);
extern int get_unused_fd(void);
extern void FASTCALL(put_unused_fd(unsigned int fd));
struct kmem_cache;

extern struct file ** alloc_fd_array(int);
extern void free_fd_array(struct file **, int);

extern fd_set *alloc_fdset(int);
extern void free_fdset(fd_set *, int);

extern int expand_files(struct files_struct *, int nr);
extern void free_fdtable(struct fdtable *fdt);
extern void __init files_defer_init(void);

static inline struct file * fcheck_files(struct files_struct *files, unsigned int fd)
{
	struct file * file = NULL;
	struct fdtable *fdt = files_fdtable(files);

	if (fd < fdt->max_fds)
		file = rcu_dereference(fdt->fd[fd]);
	return file;
}

/*
 * Check whether the specified fd has an open file.
 */
#define fcheck(fd)	fcheck_files(current->files, fd)

extern void FASTCALL(fd_install(unsigned int fd, struct file * file));

struct task_struct;

struct files_struct *get_files_struct(struct task_struct *);
void FASTCALL(put_files_struct(struct files_struct *fs));

#endif /* __LINUX_FILE_H */
