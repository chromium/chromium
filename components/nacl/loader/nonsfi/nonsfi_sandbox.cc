// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_sandbox.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/net.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <memory>

#include "base/check_op.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/common/sandbox_init.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/system_headers/linux_futex.h"
#include "sandbox/linux/system_headers/linux_signal.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if !defined(OS_NACL_NONSFI)
#error "nonsfi_sandbox.cc must be built for nacl_helper_nonsfi."
#endif

// Chrome OS Daisy (ARM) build environment and PNaCl toolchain do not define
// MAP_STACK.
#if !defined(MAP_STACK)
# if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY)
#  define MAP_STACK 0x20000
# elif defined(ARCH_CPU_MIPS_FAMILY)
#  define MAP_STACK 0x40000
# else
// Note that, on other architectures, MAP_STACK has different value,
// though Non-SFI is not supported on such architectures.
#  error "Unknown platform."
# endif
#endif  // !defined(MAP_STACK)

#define CASES SANDBOX_BPF_DSL_CASES

using sandbox::CrashSIGSYS;
using sandbox::CrashSIGSYSClone;
using sandbox::CrashSIGSYSFutex;
using sandbox::CrashSIGSYSPrctl;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace nacl {
namespace nonsfi {
namespace {

ResultExpr RestrictFcntlCommands() {
  const Arg<int> cmd(1);
  const Arg<long> long_arg(2);

  // We allow following cases:
  // 1. F_SETFD + FD_CLOEXEC: libevent's epoll_init uses this.
  // 2. F_GETFL: Used by SetNonBlocking in
  // message_pump_libevent.cc and Channel::ChannelImpl::CreatePipe
  // in ipc_channel_posix.cc. Note that the latter does not work
  // with EPERM.
  // 3. F_SETFL: Used by evutil_make_socket_nonblocking in
  // libevent and SetNonBlocking. As the latter mix O_NONBLOCK to
  // the return value of F_GETFL, so we need to allow O_ACCMODE in
  // addition to O_NONBLOCK.
  const uint64_t kAllowedMask = O_ACCMODE | O_NONBLOCK;
  return If(AnyOf(AllOf(cmd == F_SETFD, long_arg == FD_CLOEXEC), cmd == F_GETFL,
                  AllOf(cmd == F_SETFL, (long_arg & ~kAllowedMask) == 0)),
            Allow())
      .Else(CrashSIGSYS());
}

ResultExpr RestrictClone() {
  // We allow clone only for new thread creation.
  const int kCloneFlags =
      CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
      CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID;
  const Arg<int> flags(0);
  return If(flags == kCloneFlags, Allow()).Else(CrashSIGSYSClone());
}

ResultExpr RestrictFutexOperation() {
  // TODO(hamaji): Allow only FUTEX_PRIVATE_FLAG futexes.
  const uint64_t kAllowedFutexFlags = FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME;
  const Arg<int> op(1);
  return Switch(op & ~kAllowedFutexFlags)
      .CASES((FUTEX_WAIT,
              FUTEX_WAKE,
              FUTEX_REQUEUE,
              FUTEX_CMP_REQUEUE,
              FUTEX_WAKE_OP,
              FUTEX_WAIT_BITSET,
              FUTEX_WAKE_BITSET),
             Allow())
      .Default(CrashSIGSYSFutex());
}

ResultExpr RestrictPrctl() {
  // base::PlatformThread::SetName() uses PR_SET_NAME so we return
  // EPERM for it. Otherwise, we will raise SIGSYS.
  const Arg<int> option(0);
  return If(option == PR_SET_NAME, Error(EPERM)).Else(CrashSIGSYSPrctl());
}

#if defined(__i386__)
ResultExpr RestrictSocketcall() {
  // We only allow shutdown(), sendmsg(), and recvmsg().
  const Arg<int> call(0);
  return Switch(call)
      .CASES((SYS_SHUTDOWN, SYS_SENDMSG, SYS_RECVMSG), Allow())
      .Default(CrashSIGSYS());
}
#endif

ResultExpr RestrictMprotect() {
  // TODO(jln, keescook, drewry): Limit the use of mprotect by adding
  // some features to linux kernel.
  const uint64_t kAllowedMask = PROT_READ | PROT_WRITE | PROT_EXEC;
  const Arg<int> prot(2);
  return If((prot & ~kAllowedMask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictMmap() {
  const uint64_t kAllowedFlagMask =
      MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_FIXED;
  // When PROT_EXEC is specified, IRT mmap of Non-SFI NaCl helper
  // calls mmap without PROT_EXEC and then adds PROT_EXEC by mprotect,
  // so we do not need to allow PROT_EXEC in mmap.
  const uint64_t kAllowedProtMask = PROT_READ | PROT_WRITE;
  const Arg<int> prot(2), flags(3);
  return If(AllOf((prot & ~kAllowedProtMask) == 0,
                  (flags & ~kAllowedFlagMask) == 0),
            Allow())
      .Else(CrashSIGSYS());
}

ResultExpr RestrictTgkill(int policy_pid) {
  const Arg<int> tgid(0), tid(1), signum(2);
  // Only sending SIGUSR1 to a thread in the same process is allowed.
  return If(AllOf(
                tgid == policy_pid,
                // Arg does not support a greater-than operator, so two separate
                // checks are needed to ensure tid is positive.
                tid != 0,
                (tid & (1u << 31)) == 0,  // tid is non-negative.
                signum == LINUX_SIGUSR1),
            Allow())
      .Else(CrashSIGSYS());
}

bool IsGracefullyDenied(int sysno) {
  switch (sysno) {
    // libevent tries this first and then falls back to poll if
    // epoll_create fails.
    case __NR_epoll_create:
    // third_party/libevent uses them, but we can just return -1 from
    // them as it is just checking getuid() != geteuid() and
    // getgid() != getegid()
#if defined(__i386__) || defined(__arm__)
    case __NR_getegid32:
    case __NR_geteuid32:
    case __NR_getgid32:
    case __NR_getuid32:
#endif
    case __NR_getegid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getuid:
    // tcmalloc calls madvise in TCMalloc_SystemRelease.
    case __NR_madvise:
    // EPERM instead of SIGSYS as glibc tries to open files in /proc.
    // openat via opendir via get_nprocs_conf and open via get_nprocs.
    // TODO(hamaji): Remove this when we switch to newlib.
    case __NR_open:
    case __NR_openat:
    // For RunSandboxSanityChecks().
    case __NR_ptrace:
    // glibc uses this for its pthread implementation. If we return
    // EPERM for this, glibc will stop using this.
    // TODO(hamaji): newlib does not use this. Make this SIGTRAP once
    // we have switched to newlib.
    case __NR_set_robust_list:
    // This is obsolete in ARM EABI, but x86 glibc indirectly calls
    // this in sysconf.
#if defined(__i386__) || defined(__x86_64__)
    case __NR_time:
#endif
      return true;

    default:
      return false;
  }
}

void RunSandboxSanityChecks() {
  errno = 0;
  // Make a ptrace request with an invalid PID.
  long ptrace_ret = syscall(
      __NR_ptrace, 3 /* = PTRACE_PEEKUSER */, -1 /* pid */, NULL, NULL);
  CHECK_EQ(-1, ptrace_ret);
  // Without the sandbox on, this ptrace call would ESRCH instead.
  CHECK_EQ(EPERM, errno);
}

}  // namespace

NaClNonSfiBPFSandboxPolicy::NaClNonSfiBPFSandboxPolicy()
    : policy_pid_(getpid()) {
}

NaClNonSfiBPFSandboxPolicy::~NaClNonSfiBPFSandboxPolicy() {
  // Make sure that this policy is created, used and destroyed by a single
  // process.
  DCHECK_EQ(getpid(), policy_pid_);
}

ResultExpr NaClNonSfiBPFSandboxPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    // Allowed syscalls.
#if defined(__i386__) || defined(__arm__)
    case __NR__llseek:
#elif defined(__x86_64__)
    case __NR_lseek:
#endif
    case __NR_close:
    case __NR_dup:
    case __NR_dup2:
    case __NR_exit:
    case __NR_exit_group:
#if defined(__i386__) || defined(__arm__)
    case __NR_fstat64:
#elif defined(__x86_64__)
    case __NR_fstat:
#endif
    // TODO(hamaji): Remove the need of gettid. Currently, this is
    // called from PlatformThread::CurrentId().
    case __NR_gettid:
    case __NR_gettimeofday:
    case __NR_munmap:
    case __NR_nanosleep:
    // TODO(hamaji): Remove the need of pipe. Currently, this is
    // called from base::MessagePumpLibevent::Init().
    case __NR_pipe:
    case __NR_poll:
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_read:
    case __NR_restart_syscall:
    case __NR_sched_yield:
    // __NR_times needed as clock() is called by CommandBufferHelper, which is
    // used by NaCl applications that use Pepper's 3D interfaces.
    // See crbug.com/264856 for details.
    case __NR_times:
    case __NR_write:
#if defined(__arm__)
    case __ARM_NR_cacheflush:
#endif
      return Allow();

    case __NR_clock_getres:
    case __NR_clock_gettime:
      return sandbox::RestrictClockID();

    case __NR_clone:
      return RestrictClone();

#if defined(__x86_64__)
    case __NR_fcntl:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_fcntl64:
#endif
      return RestrictFcntlCommands();

    case __NR_futex:
      return RestrictFutexOperation();

#if defined(__x86_64__)
    case __NR_mmap:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_mmap2:
#endif
      return RestrictMmap();
    case __NR_mprotect:
      return RestrictMprotect();

    case __NR_prctl:
      return RestrictPrctl();

#if defined(__i386__)
    case __NR_socketcall:
      return RestrictSocketcall();
#endif
#if defined(__x86_64__) || defined(__arm__)
    case __NR_recvmsg:
    case __NR_sendmsg:
    case __NR_shutdown:
      return Allow();
#endif

    case __NR_tgkill:
      return RestrictTgkill(policy_pid_);

    case __NR_brk:
      // The behavior of brk on Linux is different from other system
      // calls. It does not return errno but the current break on
      // failure. glibc thinks brk failed if the return value of brk
      // is less than the requested address (i.e., brk(addr) < addr).
      // So, glibc thinks brk succeeded if we return -EPERM and we
      // need to return zero instead.
      return Error(0);

    default:
      if (IsGracefullyDenied(sysno))
        return Error(EPERM);
      return CrashSIGSYS();
  }
}

ResultExpr NaClNonSfiBPFSandboxPolicy::InvalidSyscall() const {
  return CrashSIGSYS();
}

bool InitializeBPFSandbox(base::ScopedFD proc_fd) {
  bool sandbox_is_initialized = content::InitializeSandbox(
      std::unique_ptr<sandbox::bpf_dsl::Policy>(
          new nacl::nonsfi::NaClNonSfiBPFSandboxPolicy()),
      std::move(proc_fd));
  if (!sandbox_is_initialized)
    return false;
  RunSandboxSanityChecks();
  return true;
}

}  // namespace nonsfi
}  // namespace nacl
