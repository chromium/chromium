// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/sandbox_linux/nacl_bpf_sandbox_linux.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "sandbox/sandbox_buildflags.h"

#if BUILDFLAG(USE_SECCOMP_BPF)

#include <errno.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/nacl/common/nacl_switches.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_seccomp_bpf_linux.h"

#endif  // BUILDFLAG(USE_SECCOMP_BPF)

namespace nacl {

#if BUILDFLAG(USE_SECCOMP_BPF)

namespace {

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

class NaClBPFSandboxPolicy : public sandbox::bpf_dsl::Policy {
 public:
  NaClBPFSandboxPolicy()
      : baseline_policy_(
            sandbox::policy::SandboxSeccompBPF::GetBaselinePolicy()),
        policy_pid_(syscall(__NR_getpid)) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    // nacl_process_host.cc doesn't always enable the debug stub when
    // kEnableNaClDebug is passed, but it's OK to enable the extra syscalls
    // whenever kEnableNaClDebug is passed.
    enable_nacl_debug_ = command_line->HasSwitch(switches::kEnableNaClDebug);
  }

  NaClBPFSandboxPolicy(const NaClBPFSandboxPolicy&) = delete;
  NaClBPFSandboxPolicy& operator=(const NaClBPFSandboxPolicy&) = delete;

  ~NaClBPFSandboxPolicy() override = default;

  ResultExpr EvaluateSyscall(int system_call_number) const override;
  ResultExpr InvalidSyscall() const override {
    return baseline_policy_->InvalidSyscall();
  }

 private:
  std::unique_ptr<sandbox::bpf_dsl::Policy> baseline_policy_;
  bool enable_nacl_debug_;
  const pid_t policy_pid_;
};

ResultExpr NaClBPFSandboxPolicy::EvaluateSyscall(int sysno) const {
  DCHECK(baseline_policy_);

  // EvaluateSyscall must be called from the same process that instantiated the
  // NaClBPFSandboxPolicy.
  DCHECK_EQ(policy_pid_, syscall(__NR_getpid));

  // NaCl's GDB debug stub uses the following socket system calls. We only
  // allow them when --enable-nacl-debug is specified.
  if (enable_nacl_debug_) {
    switch (sysno) {
    // trusted/service_runtime/linux/thread_suspension.c needs sigwait(). Thread
    // suspension is currently only used in the debug stub.
      case __NR_rt_sigtimedwait:
        return Allow();
#if defined(__x86_64__) || defined(__arm__) || defined(__mips__)
      // transport_common.cc needs this.
      case __NR_accept:
      case __NR_setsockopt:
        return Allow();
#elif defined(__i386__)
      case __NR_socketcall:
        return Allow();
#endif
      default:
        break;
    }
  }

  switch (sysno) {
#if defined(__i386__) || defined(__mips__)
    // Needed on i386 to set-up the custom segments.
    case __NR_modify_ldt:
#endif
    // NaCl uses custom signal stacks.
    case __NR_sigaltstack:
    // Below is fairly similar to the policy for a Chromium renderer.
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    // NaCl runtime uses flock to simulate POSIX behavior for pwrite.
    case __NR_flock:
    case __NR_pwrite64:
    // set_robust_list(2) is generating quite a bit of logspam on Chrome OS
    // (and probably on Linux too), and per its manpage it should never EPERM.
    // Moreover, it also doesn't allow affecting other processes, since it
    // doesn't take a |pid| argument.
    // See crbug.com/1051197 for details.
    case __NR_set_robust_list:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sysinfo:
    // __NR_times needed as clock() is called by CommandBufferHelper, which is
    // used by NaCl applications that use Pepper's 3D interfaces.
    // See crbug.com/264856 for details.
    case __NR_times:
    case __NR_uname:
      return Allow();
    case __NR_ioctl:
    case __NR_ptrace:
      return Error(EPERM);
    case __NR_sched_getaffinity:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
      return sandbox::RestrictSchedTarget(policy_pid_, sysno);
    // NaClAddrSpaceBeforeAlloc needs prlimit64.
    case __NR_prlimit64:
      return sandbox::RestrictPrlimit64(policy_pid_);
    // NaCl runtime exposes clock_getres to untrusted code.
    case __NR_clock_getres:
      return sandbox::RestrictClockID();
    default:
      return baseline_policy_->EvaluateSyscall(sysno);
  }
  NOTREACHED_IN_MIGRATION();
  // GCC wants this.
  return Error(EPERM);
}

void RunSandboxSanityChecks() {
  errno = 0;
  // Make a ptrace request with an invalid PID.
  long ptrace_ret = ptrace(PTRACE_PEEKUSER, -1 /* pid */, NULL, NULL);
  CHECK_EQ(-1, ptrace_ret);
  // Without the sandbox on, this ptrace call would ESRCH instead.
  CHECK_EQ(EPERM, errno);
}

}  // namespace

#else

#error "Seccomp-bpf disabled on supported architecture!"

#endif  // BUILDFLAG(USE_SECCOMP_BPF)

bool InitializeBPFSandbox(base::ScopedFD proc_fd) {
#if BUILDFLAG(USE_SECCOMP_BPF)
  if (sandbox::policy::SandboxSeccompBPF::StartSandboxWithExternalPolicy(
          std::make_unique<NaClBPFSandboxPolicy>(), std::move(proc_fd))) {
    RunSandboxSanityChecks();
    return true;
  }
#endif  // BUILDFLAG(USE_SECCOMP_BPF)
  return false;
}

}  // namespace nacl
