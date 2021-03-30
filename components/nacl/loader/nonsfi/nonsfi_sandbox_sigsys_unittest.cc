// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sanitizers internally use some syscalls which non-SFI NaCl disallows.
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER) && !defined(LEAK_SANITIZER)

#include "components/nacl/loader/nonsfi/nonsfi_sandbox.h"

#include <sys/syscall.h>
#include <unistd.h>

#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace {

// Test cases in this file just make sure syscalls not in the allow list
// are appropriately disallowed. They should raise SIGSYS regardless
// of arguments. We always pass five zeros not to pass uninitialized
// values to syscalls.
#define RESTRICT_SYSCALL_DEATH_TEST_IMPL(name, sysno)                   \
  BPF_DEATH_TEST_C(NaClNonSfiSandboxSIGSYSTest,                         \
                   name,                                                \
                   DEATH_SEGV_MESSAGE(                                  \
                       sandbox::GetErrorMessageContentForTests()),      \
                   nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {          \
    syscall(sysno, 0, 0, 0, 0, 0, 0);                                   \
  }

#define RESTRICT_SYSCALL_DEATH_TEST(name)               \
  RESTRICT_SYSCALL_DEATH_TEST_IMPL(name, __NR_ ## name)

#define RESTRICT_ARM_SYSCALL_DEATH_TEST(name)           \
  RESTRICT_SYSCALL_DEATH_TEST_IMPL(ARM_ ## name, __ARM_NR_ ## name)

#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(_newselect);
#endif
RESTRICT_SYSCALL_DEATH_TEST(_sysctl);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(accept);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(accept4);
#endif
RESTRICT_SYSCALL_DEATH_TEST(access);
RESTRICT_SYSCALL_DEATH_TEST(acct);
RESTRICT_SYSCALL_DEATH_TEST(add_key);
RESTRICT_SYSCALL_DEATH_TEST(adjtimex);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(afs_syscall);
#endif
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(alarm);
#endif
#if defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(arch_prctl);
#endif
#if defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(arm_fadvise64_64);
#endif
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(bdflush);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(bind);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(break);
#endif
RESTRICT_SYSCALL_DEATH_TEST(capget);
RESTRICT_SYSCALL_DEATH_TEST(capset);
RESTRICT_SYSCALL_DEATH_TEST(chdir);
RESTRICT_SYSCALL_DEATH_TEST(chmod);
RESTRICT_SYSCALL_DEATH_TEST(chown);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(chown32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(chroot);
RESTRICT_SYSCALL_DEATH_TEST(clock_adjtime);
RESTRICT_SYSCALL_DEATH_TEST(clock_nanosleep);
RESTRICT_SYSCALL_DEATH_TEST(clock_settime);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(connect);
#endif
RESTRICT_SYSCALL_DEATH_TEST(creat);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(create_module);
#endif
RESTRICT_SYSCALL_DEATH_TEST(delete_module);
RESTRICT_SYSCALL_DEATH_TEST(dup3);
RESTRICT_SYSCALL_DEATH_TEST(epoll_create1);
RESTRICT_SYSCALL_DEATH_TEST(epoll_ctl);
#if defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(epoll_ctl_old);
#endif
RESTRICT_SYSCALL_DEATH_TEST(epoll_pwait);
RESTRICT_SYSCALL_DEATH_TEST(epoll_wait);
#if defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(epoll_wait_old);
#endif
RESTRICT_SYSCALL_DEATH_TEST(eventfd);
RESTRICT_SYSCALL_DEATH_TEST(eventfd2);
RESTRICT_SYSCALL_DEATH_TEST(execve);
RESTRICT_SYSCALL_DEATH_TEST(faccessat);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(fadvise64);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(fadvise64_64);
#endif
RESTRICT_SYSCALL_DEATH_TEST(fallocate);
RESTRICT_SYSCALL_DEATH_TEST(fanotify_init);
RESTRICT_SYSCALL_DEATH_TEST(fanotify_mark);
RESTRICT_SYSCALL_DEATH_TEST(fchdir);
RESTRICT_SYSCALL_DEATH_TEST(fchmod);
RESTRICT_SYSCALL_DEATH_TEST(fchmodat);
RESTRICT_SYSCALL_DEATH_TEST(fchown);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(fchown32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(fchownat);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(fcntl);
#endif
RESTRICT_SYSCALL_DEATH_TEST(fdatasync);
RESTRICT_SYSCALL_DEATH_TEST(fgetxattr);
RESTRICT_SYSCALL_DEATH_TEST(flistxattr);
RESTRICT_SYSCALL_DEATH_TEST(flock);
RESTRICT_SYSCALL_DEATH_TEST(fork);
RESTRICT_SYSCALL_DEATH_TEST(fremovexattr);
RESTRICT_SYSCALL_DEATH_TEST(fsetxattr);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(fstat);
#endif
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(fstatat64);
#endif
RESTRICT_SYSCALL_DEATH_TEST(fstatfs);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(fstatfs64);
#endif
RESTRICT_SYSCALL_DEATH_TEST(fsync);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(ftime);
#endif
RESTRICT_SYSCALL_DEATH_TEST(ftruncate);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(ftruncate64);
#endif
RESTRICT_SYSCALL_DEATH_TEST(futimesat);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(get_kernel_syms);
#endif
RESTRICT_SYSCALL_DEATH_TEST(get_mempolicy);
RESTRICT_SYSCALL_DEATH_TEST(get_robust_list);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(get_thread_area);
#endif
RESTRICT_SYSCALL_DEATH_TEST(getcpu);
RESTRICT_SYSCALL_DEATH_TEST(getcwd);
RESTRICT_SYSCALL_DEATH_TEST(getdents);
RESTRICT_SYSCALL_DEATH_TEST(getdents64);
RESTRICT_SYSCALL_DEATH_TEST(getgroups);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(getgroups32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(getitimer);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(getpeername);
#endif
RESTRICT_SYSCALL_DEATH_TEST(getpgid);
RESTRICT_SYSCALL_DEATH_TEST(getpgrp);
RESTRICT_SYSCALL_DEATH_TEST(getpid);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(getpmsg);
#endif
RESTRICT_SYSCALL_DEATH_TEST(getppid);
RESTRICT_SYSCALL_DEATH_TEST(getpriority);
RESTRICT_SYSCALL_DEATH_TEST(getresgid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(getresgid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(getresuid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(getresuid32);
#endif
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(getrlimit);
#endif
RESTRICT_SYSCALL_DEATH_TEST(getrusage);
RESTRICT_SYSCALL_DEATH_TEST(getsid);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(getsockname);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(getsockopt);
#endif
RESTRICT_SYSCALL_DEATH_TEST(getxattr);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(gtty);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(idle);
#endif
RESTRICT_SYSCALL_DEATH_TEST(init_module);
RESTRICT_SYSCALL_DEATH_TEST(inotify_add_watch);
RESTRICT_SYSCALL_DEATH_TEST(inotify_init);
RESTRICT_SYSCALL_DEATH_TEST(inotify_init1);
RESTRICT_SYSCALL_DEATH_TEST(inotify_rm_watch);
RESTRICT_SYSCALL_DEATH_TEST(io_cancel);
RESTRICT_SYSCALL_DEATH_TEST(io_destroy);
RESTRICT_SYSCALL_DEATH_TEST(io_getevents);
RESTRICT_SYSCALL_DEATH_TEST(io_setup);
RESTRICT_SYSCALL_DEATH_TEST(io_submit);
RESTRICT_SYSCALL_DEATH_TEST(ioctl);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(ioperm);
#endif
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(iopl);
#endif
RESTRICT_SYSCALL_DEATH_TEST(ioprio_get);
RESTRICT_SYSCALL_DEATH_TEST(ioprio_set);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(ipc);
#endif
RESTRICT_SYSCALL_DEATH_TEST(kexec_load);
RESTRICT_SYSCALL_DEATH_TEST(keyctl);
RESTRICT_SYSCALL_DEATH_TEST(kill);
RESTRICT_SYSCALL_DEATH_TEST(lchown);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(lchown32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(lgetxattr);
RESTRICT_SYSCALL_DEATH_TEST(link);
RESTRICT_SYSCALL_DEATH_TEST(linkat);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(listen);
#endif
RESTRICT_SYSCALL_DEATH_TEST(listxattr);
RESTRICT_SYSCALL_DEATH_TEST(llistxattr);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(lock);
#endif
RESTRICT_SYSCALL_DEATH_TEST(lookup_dcookie);
RESTRICT_SYSCALL_DEATH_TEST(lremovexattr);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(lseek);
#endif
RESTRICT_SYSCALL_DEATH_TEST(lsetxattr);
RESTRICT_SYSCALL_DEATH_TEST(lstat);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(lstat64);
#endif
RESTRICT_SYSCALL_DEATH_TEST(mbind);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(migrate_pages);
#endif
RESTRICT_SYSCALL_DEATH_TEST(mincore);
RESTRICT_SYSCALL_DEATH_TEST(mkdir);
RESTRICT_SYSCALL_DEATH_TEST(mkdirat);
RESTRICT_SYSCALL_DEATH_TEST(mknod);
RESTRICT_SYSCALL_DEATH_TEST(mknodat);
RESTRICT_SYSCALL_DEATH_TEST(mlock);
RESTRICT_SYSCALL_DEATH_TEST(mlockall);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(mmap);
#endif
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(modify_ldt);
#endif
RESTRICT_SYSCALL_DEATH_TEST(mount);
RESTRICT_SYSCALL_DEATH_TEST(move_pages);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(mpx);
#endif
RESTRICT_SYSCALL_DEATH_TEST(mq_getsetattr);
RESTRICT_SYSCALL_DEATH_TEST(mq_notify);
RESTRICT_SYSCALL_DEATH_TEST(mq_open);
RESTRICT_SYSCALL_DEATH_TEST(mq_timedreceive);
RESTRICT_SYSCALL_DEATH_TEST(mq_timedsend);
RESTRICT_SYSCALL_DEATH_TEST(mq_unlink);
RESTRICT_SYSCALL_DEATH_TEST(mremap);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(msgctl);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(msgget);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(msgrcv);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(msgsnd);
#endif
RESTRICT_SYSCALL_DEATH_TEST(msync);
RESTRICT_SYSCALL_DEATH_TEST(munlock);
RESTRICT_SYSCALL_DEATH_TEST(munlockall);
RESTRICT_SYSCALL_DEATH_TEST(name_to_handle_at);
#if defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(newfstatat);
#endif
RESTRICT_SYSCALL_DEATH_TEST(nfsservctl);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(nice);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(oldfstat);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(oldlstat);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(oldolduname);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(oldstat);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(olduname);
#endif
RESTRICT_SYSCALL_DEATH_TEST(open_by_handle_at);
RESTRICT_SYSCALL_DEATH_TEST(pause);
#if defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(pciconfig_iobase);
#endif
#if defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(pciconfig_read);
#endif
#if defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(pciconfig_write);
#endif
RESTRICT_SYSCALL_DEATH_TEST(perf_event_open);
RESTRICT_SYSCALL_DEATH_TEST(personality);
RESTRICT_SYSCALL_DEATH_TEST(pipe2);
RESTRICT_SYSCALL_DEATH_TEST(pivot_root);
RESTRICT_SYSCALL_DEATH_TEST(ppoll);
RESTRICT_SYSCALL_DEATH_TEST(preadv);
RESTRICT_SYSCALL_DEATH_TEST(prlimit64);
RESTRICT_SYSCALL_DEATH_TEST(process_vm_readv);
RESTRICT_SYSCALL_DEATH_TEST(process_vm_writev);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(prof);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(profil);
#endif
RESTRICT_SYSCALL_DEATH_TEST(pselect6);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(putpmsg);
#endif
RESTRICT_SYSCALL_DEATH_TEST(pwritev);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(query_module);
#endif
RESTRICT_SYSCALL_DEATH_TEST(quotactl);
RESTRICT_SYSCALL_DEATH_TEST(readahead);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(readdir);
#endif
RESTRICT_SYSCALL_DEATH_TEST(readlink);
RESTRICT_SYSCALL_DEATH_TEST(readlinkat);
RESTRICT_SYSCALL_DEATH_TEST(readv);
RESTRICT_SYSCALL_DEATH_TEST(reboot);
#if defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(recv);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(recvfrom);
#endif
RESTRICT_SYSCALL_DEATH_TEST(recvmmsg);
RESTRICT_SYSCALL_DEATH_TEST(remap_file_pages);
RESTRICT_SYSCALL_DEATH_TEST(removexattr);
RESTRICT_SYSCALL_DEATH_TEST(rename);
RESTRICT_SYSCALL_DEATH_TEST(renameat);
RESTRICT_SYSCALL_DEATH_TEST(request_key);
RESTRICT_SYSCALL_DEATH_TEST(rmdir);
RESTRICT_SYSCALL_DEATH_TEST(rt_sigaction);
RESTRICT_SYSCALL_DEATH_TEST(rt_sigpending);
RESTRICT_SYSCALL_DEATH_TEST(rt_sigprocmask);
RESTRICT_SYSCALL_DEATH_TEST(rt_sigqueueinfo);
RESTRICT_SYSCALL_DEATH_TEST(rt_sigreturn);
RESTRICT_SYSCALL_DEATH_TEST(rt_sigsuspend);
RESTRICT_SYSCALL_DEATH_TEST(rt_sigtimedwait);
RESTRICT_SYSCALL_DEATH_TEST(rt_tgsigqueueinfo);
RESTRICT_SYSCALL_DEATH_TEST(sched_get_priority_max);
RESTRICT_SYSCALL_DEATH_TEST(sched_get_priority_min);
RESTRICT_SYSCALL_DEATH_TEST(sched_getaffinity);
RESTRICT_SYSCALL_DEATH_TEST(sched_getparam);
RESTRICT_SYSCALL_DEATH_TEST(sched_getscheduler);
RESTRICT_SYSCALL_DEATH_TEST(sched_rr_get_interval);
RESTRICT_SYSCALL_DEATH_TEST(sched_setaffinity);
RESTRICT_SYSCALL_DEATH_TEST(sched_setparam);
RESTRICT_SYSCALL_DEATH_TEST(sched_setscheduler);
#if defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(security);
#endif
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(select);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(semctl);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(semget);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(semop);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(semtimedop);
#endif
#if defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(send);
#endif
RESTRICT_SYSCALL_DEATH_TEST(sendfile);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sendfile64);
#endif
RESTRICT_SYSCALL_DEATH_TEST(sendmmsg);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sendto);
#endif
RESTRICT_SYSCALL_DEATH_TEST(set_mempolicy);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(set_thread_area);
#endif
RESTRICT_SYSCALL_DEATH_TEST(set_tid_address);
RESTRICT_SYSCALL_DEATH_TEST(setdomainname);
RESTRICT_SYSCALL_DEATH_TEST(setfsgid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setfsgid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setfsuid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setfsuid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setgid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setgid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setgroups);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setgroups32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(sethostname);
RESTRICT_SYSCALL_DEATH_TEST(setitimer);
RESTRICT_SYSCALL_DEATH_TEST(setns);
RESTRICT_SYSCALL_DEATH_TEST(setpgid);
RESTRICT_SYSCALL_DEATH_TEST(setpriority);
RESTRICT_SYSCALL_DEATH_TEST(setregid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setregid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setresgid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setresgid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setresuid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setresuid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setreuid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setreuid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setrlimit);
RESTRICT_SYSCALL_DEATH_TEST(setsid);
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setsockopt);
#endif
RESTRICT_SYSCALL_DEATH_TEST(settimeofday);
RESTRICT_SYSCALL_DEATH_TEST(setuid);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(setuid32);
#endif
RESTRICT_SYSCALL_DEATH_TEST(setxattr);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(sgetmask);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(shmat);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(shmctl);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(shmdt);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(shmget);
#endif
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sigaction);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(signal);
#endif
RESTRICT_SYSCALL_DEATH_TEST(signalfd);
RESTRICT_SYSCALL_DEATH_TEST(signalfd4);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sigpending);
#endif
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sigprocmask);
#endif
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sigreturn);
#endif
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sigsuspend);
#endif
#if defined(__x86_64__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(socket);
#endif
RESTRICT_SYSCALL_DEATH_TEST(splice);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(ssetmask);
#endif
RESTRICT_SYSCALL_DEATH_TEST(stat);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(stat64);
#endif
RESTRICT_SYSCALL_DEATH_TEST(statfs);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(statfs64);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(stime);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(stty);
#endif
RESTRICT_SYSCALL_DEATH_TEST(swapoff);
RESTRICT_SYSCALL_DEATH_TEST(swapon);
RESTRICT_SYSCALL_DEATH_TEST(symlink);
RESTRICT_SYSCALL_DEATH_TEST(symlinkat);
RESTRICT_SYSCALL_DEATH_TEST(sync);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(sync_file_range);
#endif
#if defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(sync_file_range2);
#endif
RESTRICT_SYSCALL_DEATH_TEST(syncfs);
RESTRICT_SYSCALL_DEATH_TEST(sysfs);
RESTRICT_SYSCALL_DEATH_TEST(sysinfo);
RESTRICT_SYSCALL_DEATH_TEST(syslog);
RESTRICT_SYSCALL_DEATH_TEST(tee);
RESTRICT_SYSCALL_DEATH_TEST(tgkill);
RESTRICT_SYSCALL_DEATH_TEST(timer_create);
RESTRICT_SYSCALL_DEATH_TEST(timer_delete);
RESTRICT_SYSCALL_DEATH_TEST(timer_getoverrun);
RESTRICT_SYSCALL_DEATH_TEST(timer_gettime);
RESTRICT_SYSCALL_DEATH_TEST(timer_settime);
RESTRICT_SYSCALL_DEATH_TEST(timerfd_create);
RESTRICT_SYSCALL_DEATH_TEST(timerfd_gettime);
RESTRICT_SYSCALL_DEATH_TEST(timerfd_settime);
RESTRICT_SYSCALL_DEATH_TEST(tkill);
RESTRICT_SYSCALL_DEATH_TEST(truncate);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(truncate64);
#endif
#if defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(tuxcall);
#endif
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_DEATH_TEST(ugetrlimit);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(ulimit);
#endif
RESTRICT_SYSCALL_DEATH_TEST(umask);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(umount);
#endif
RESTRICT_SYSCALL_DEATH_TEST(umount2);
RESTRICT_SYSCALL_DEATH_TEST(uname);
RESTRICT_SYSCALL_DEATH_TEST(unlink);
RESTRICT_SYSCALL_DEATH_TEST(unlinkat);
RESTRICT_SYSCALL_DEATH_TEST(unshare);
RESTRICT_SYSCALL_DEATH_TEST(uselib);
RESTRICT_SYSCALL_DEATH_TEST(ustat);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_DEATH_TEST(utime);
#endif
RESTRICT_SYSCALL_DEATH_TEST(utimensat);
RESTRICT_SYSCALL_DEATH_TEST(utimes);
RESTRICT_SYSCALL_DEATH_TEST(vfork);
RESTRICT_SYSCALL_DEATH_TEST(vhangup);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(vm86);
#endif
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(vm86old);
#endif
RESTRICT_SYSCALL_DEATH_TEST(vmsplice);
RESTRICT_SYSCALL_DEATH_TEST(vserver);
RESTRICT_SYSCALL_DEATH_TEST(wait4);
RESTRICT_SYSCALL_DEATH_TEST(waitid);
#if defined(__i386__)
RESTRICT_SYSCALL_DEATH_TEST(waitpid);
#endif
RESTRICT_SYSCALL_DEATH_TEST(writev);

// ARM specific syscalls.
#if defined(__arm__)
RESTRICT_ARM_SYSCALL_DEATH_TEST(breakpoint);
RESTRICT_ARM_SYSCALL_DEATH_TEST(usr26);
RESTRICT_ARM_SYSCALL_DEATH_TEST(usr32);
RESTRICT_ARM_SYSCALL_DEATH_TEST(set_tls);
#endif

}  // namespace

#endif  // !ADDRESS_SANITIZER && !THREAD_SANITIZER &&
        // !MEMORY_SANITIZER && !LEAK_SANITIZER
