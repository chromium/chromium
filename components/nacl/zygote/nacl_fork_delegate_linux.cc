// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/nacl/zygote/nacl_fork_delegate_linux.h"

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_paths.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nacl_helper_linux.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "sandbox/linux/suid/client/setuid_sandbox_host.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "sandbox/policy/switches.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace {

// Note these need to match up with their counterparts in nacl_helper_linux.c
// and nacl_helper_bootstrap_linux.c.
const char kNaClHelperReservedAtZero[] =
    "--reserved_at_zero=0xXXXXXXXXXXXXXXXX";
const char kNaClHelperRDebug[] = "--r_debug=0xXXXXXXXXXXXXXXXX";

// This is an environment variable which controls which (if any) other
// environment variables are passed through to NaCl processes.  e.g.,
// NACL_ENV_PASSTHROUGH="PATH,CWD" would pass both $PATH and $CWD to the child
// process.
const char kNaClEnvPassthrough[] = "NACL_ENV_PASSTHROUGH";
char kNaClEnvPassthroughDelimiter = ',';

// The following environment variables are always passed through if they exist
// in the parent process.
const char kNaClExeStderr[] = "NACL_EXE_STDERR";
const char kNaClExeStdout[] = "NACL_EXE_STDOUT";
const char kNaClVerbosity[] = "NACLVERBOSITY";

#if defined(ARCH_CPU_X86)
bool NonZeroSegmentBaseIsSlow() {
  base::CPU cpuid;
  // Using a non-zero segment base is known to be very slow on Intel
  // Atom CPUs.  See "Segmentation-based Memory Protection Mechanism
  // on Intel Atom Microarchitecture: Coding Optimizations" (Leonardo
  // Potenza, Intel).
  //
  // The following list of CPU model numbers is taken from:
  // "Intel 64 and IA-32 Architectures Software Developer's Manual"
  // (http://download.intel.com/products/processor/manual/325462.pdf),
  // "Table 35-1. CPUID Signature Values of DisplayFamily_DisplayModel"
  // (Volume 3C, 35-1), which contains:
  //   "06_36H - Intel Atom S Processor Family
  //    06_1CH, 06_26H, 06_27H, 06_35, 06_36 - Intel Atom Processor Family"
  if (cpuid.family() == 6) {
    switch (cpuid.model()) {
      case 0x1c:
      case 0x26:
      case 0x27:
      case 0x35:
      case 0x36:
        return true;
    }
  }
  return false;
}
#endif

// Send an IPC request on |ipc_channel|. The request is contained in
// |request_pickle| and can have file descriptors attached in |attached_fds|.
// |reply_data_buffer| must be allocated by the caller and will contain the
// reply. The size of the reply will be written to |reply_size|.
// This code assumes that only one thread can write to |ipc_channel| to make
// requests.
bool SendIPCRequestAndReadReply(int ipc_channel,
                                const std::vector<int>& attached_fds,
                                const base::Pickle& request_pickle,
                                char* reply_data_buffer,
                                size_t reply_data_buffer_size,
                                ssize_t* reply_size) {
  DCHECK_LE(static_cast<size_t>(kNaClMaxIPCMessageLength),
            reply_data_buffer_size);
  DCHECK(reply_size);

  if (!base::UnixDomainSocket::SendMsg(ipc_channel, request_pickle.data(),
                                       request_pickle.size(), attached_fds)) {
    LOG(ERROR) << "SendIPCRequestAndReadReply: SendMsg failed";
    return false;
  }

  // Then read the remote reply.
  std::vector<base::ScopedFD> received_fds;
  const ssize_t msg_len =
      base::UnixDomainSocket::RecvMsg(ipc_channel, reply_data_buffer,
                                      reply_data_buffer_size, &received_fds);
  if (msg_len <= 0) {
    LOG(ERROR) << "SendIPCRequestAndReadReply: RecvMsg failed";
    return false;
  }
  *reply_size = msg_len;
  return true;
}

}  // namespace.

namespace nacl {

void AddNaClZygoteForkDelegates(
    std::vector<std::unique_ptr<content::ZygoteForkDelegate>>* delegates) {
  // We don't need the delegates for the unsandboxed zygote since NaCl always
  // starts from the sandboxed zygote.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kNoZygoteSandbox)) {
    return;
  }

  delegates->push_back(std::make_unique<NaClForkDelegate>());
}

NaClForkDelegate::NaClForkDelegate() : status_(kNaClHelperUnused), fd_(-1) {}

void NaClForkDelegate::Init(const int sandboxdesc,
                            const bool enable_layer1_sandbox) {
  VLOG(1) << "NaClForkDelegate::Init()";

  // TODO(rickyz): Make IsSuidSandboxChild a static function.
  std::unique_ptr<sandbox::SetuidSandboxClient> setuid_sandbox_client(
      sandbox::SetuidSandboxClient::Create());
  const bool using_setuid_sandbox = setuid_sandbox_client->IsSuidSandboxChild();
  const bool using_namespace_sandbox =
      sandbox::NamespaceSandbox::InNewUserNamespace();

  CHECK(!(using_setuid_sandbox && using_namespace_sandbox));
  if (enable_layer1_sandbox) {
    CHECK(using_setuid_sandbox || using_namespace_sandbox);
  }

  std::unique_ptr<sandbox::SetuidSandboxHost> setuid_sandbox_host(
      sandbox::SetuidSandboxHost::Create());

  // For communications between the NaCl loader process and
  // the browser process.
  int nacl_sandbox_descriptor =
      base::GlobalDescriptors::kBaseDescriptor + kSandboxIPCChannel;
  // Confirm a hard-wired assumption.
  DCHECK_EQ(sandboxdesc, nacl_sandbox_descriptor);

  int fds[2];
  PCHECK(0 == socketpair(PF_UNIX, SOCK_SEQPACKET, 0, fds));

  bool use_nacl_bootstrap = true;
#if defined(ARCH_CPU_X86_64)
  // Using nacl_helper_bootstrap is not necessary on x86-64 because
  // NaCl's x86-64 sandbox is not zero-address-based.  Starting
  // nacl_helper through nacl_helper_bootstrap works on x86-64, but it
  // leaves nacl_helper_bootstrap mapped at a fixed address at the
  // bottom of the address space, which is undesirable because it
  // effectively defeats ASLR.
  use_nacl_bootstrap = false;
#elif defined(ARCH_CPU_X86)
  // Performance vs. security trade-off: We prefer using a
  // non-zero-address-based sandbox on x86-32 because it provides some
  // ASLR and so is more secure.  However, on Atom CPUs, using a
  // non-zero segment base is very slow, so we use a zero-based
  // sandbox on those.
  use_nacl_bootstrap = NonZeroSegmentBaseIsSlow();
#endif

  status_ = kNaClHelperUnused;
  base::FilePath helper_exe;
  base::FilePath helper_bootstrap_exe;
  if (!base::PathService::Get(nacl::FILE_NACL_HELPER, &helper_exe)) {
    status_ = kNaClHelperMissing;
  } else if (use_nacl_bootstrap &&
             !base::PathService::Get(nacl::FILE_NACL_HELPER_BOOTSTRAP,
                                     &helper_bootstrap_exe)) {
    status_ = kNaClHelperBootstrapMissing;
  } else {
    base::CommandLine::StringVector argv_to_launch;
    {
      base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
      if (use_nacl_bootstrap)
        cmd_line.SetProgram(helper_bootstrap_exe);
      else
        cmd_line.SetProgram(helper_exe);

      // Append any switches that need to be forwarded to the NaCl helper.
      static constexpr const char* kForwardSwitches[] = {
          sandbox::policy::switches::kAllowSandboxDebugging,
          sandbox::policy::switches::kDisableSeccompFilterSandbox,
          sandbox::policy::switches::kNoSandbox,
          switches::kEnableNaClDebug,
          switches::kVerboseLoggingInNacl,
          chromeos::switches::kFeatureFlags,
      };
      const base::CommandLine& current_cmd_line =
          *base::CommandLine::ForCurrentProcess();
      cmd_line.CopySwitchesFrom(current_cmd_line, kForwardSwitches);

      // The command line needs to be tightly controlled to use
      // |helper_bootstrap_exe|. So from now on, argv_to_launch should be
      // modified directly.
      argv_to_launch = cmd_line.argv();
    }
    if (use_nacl_bootstrap) {
      // Arguments to the bootstrap helper which need to be at the start
      // of the command line, right after the helper's path.
      base::CommandLine::StringVector bootstrap_prepend;
      bootstrap_prepend.push_back(helper_exe.value());
      bootstrap_prepend.push_back(kNaClHelperReservedAtZero);
      bootstrap_prepend.push_back(kNaClHelperRDebug);
      argv_to_launch.insert(argv_to_launch.begin() + 1,
                            bootstrap_prepend.begin(),
                            bootstrap_prepend.end());
    }

    std::vector<int> max_these_limits;  // must outlive `options`
    base::LaunchOptions options;
    options.maximize_rlimits = &max_these_limits;
    options.fds_to_remap.push_back(
        std::make_pair(fds[1], kNaClZygoteDescriptor));
    options.fds_to_remap.push_back(
        std::make_pair(sandboxdesc, nacl_sandbox_descriptor));

    base::ScopedFD dummy_fd;
    if (using_setuid_sandbox) {
      // NaCl needs to keep tight control of the cmd_line, so prepend the
      // setuid sandbox wrapper manually.
      base::FilePath sandbox_path = setuid_sandbox_host->GetSandboxBinaryPath();
      argv_to_launch.insert(argv_to_launch.begin(), sandbox_path.value());
      setuid_sandbox_host->SetupLaunchOptions(&options, &dummy_fd);
      setuid_sandbox_host->SetupLaunchEnvironment();
    }

    // The NaCl processes spawned may need to exceed the ambient soft limit
    // on RLIMIT_AS to allocate the untrusted address space and its guard
    // regions.  The nacl_helper itself cannot just raise its own limit,
    // because the existing limit may prevent the initial exec of
    // nacl_helper_bootstrap from succeeding, with its large address space
    // reservation.
    max_these_limits.push_back(RLIMIT_AS);

    // Clear the environment for the NaCl Helper process.
    options.clear_environment = true;
    AddPassthroughEnvToOptions(&options);

#ifdef COMPONENT_BUILD
    // In component build, nacl_helper loads libgnutls.so.
    // Newer versions of libgnutls do implicit initialization when loaded that
    // leaves an additional /dev/urandom file descriptor open.  Passing the
    // following env var asks libgnutls not to do that implicit initialization.
    // (crbug.com/973024)
    options.environment["GNUTLS_NO_EXPLICIT_INIT"] = "1";
#endif

    base::Process process =
        using_namespace_sandbox
            ? sandbox::NamespaceSandbox::LaunchProcess(argv_to_launch, options)
            : base::LaunchProcess(argv_to_launch, options);

    if (!process.IsValid())
      status_ = kNaClHelperLaunchFailed;
    // parent and error cases are handled below

    if (using_setuid_sandbox) {
      // Sanity check that dummy_fd was kept alive for LaunchProcess.
      DCHECK(dummy_fd.is_valid());
    }
  }
  if (IGNORE_EINTR(close(fds[1])) != 0)
    LOG(ERROR) << "close(fds[1]) failed";
  if (status_ == kNaClHelperUnused) {
    constexpr ssize_t kExpectedLength = sizeof(kNaClHelperStartupAck) - 1;
    char buf[kExpectedLength];

    // Wait for ack from nacl_helper, indicating it is ready to help
    const ssize_t nread = HANDLE_EINTR(read(fds[0], buf, sizeof(buf)));
    if (nread == kExpectedLength &&
        memcmp(buf, kNaClHelperStartupAck, nread) == 0) {
      // all is well
      status_ = kNaClHelperSuccess;
      fd_ = fds[0];
      return;
    }

    status_ = kNaClHelperAckFailed;
    LOG(ERROR) << "Bad NaCl helper startup ack (" << nread << " bytes)";
  }
  // TODO(bradchen): Make this LOG(ERROR) when the NaCl helper
  // becomes the default.
  fd_ = -1;
  if (IGNORE_EINTR(close(fds[0])) != 0)
    LOG(ERROR) << "close(fds[0]) failed";
}

void NaClForkDelegate::InitialUMA(std::string* uma_name,
                                  int* uma_sample,
                                  int* uma_boundary_value) {
  *uma_name = "NaCl.Client.Helper.InitState";
  *uma_sample = status_;
  *uma_boundary_value = kNaClHelperStatusBoundary;
}

NaClForkDelegate::~NaClForkDelegate() {
  // side effect of close: delegate process will terminate
  if (status_ == kNaClHelperSuccess) {
    if (IGNORE_EINTR(close(fd_)) != 0)
      LOG(ERROR) << "close(fd_) failed";
  }
}

bool NaClForkDelegate::CanHelp(const std::string& process_type,
                               std::string* uma_name,
                               int* uma_sample,
                               int* uma_boundary_value) {
  if (process_type != switches::kNaClLoaderProcess)
    return false;
  *uma_name = "NaCl.Client.Helper.StateOnFork";
  *uma_sample = status_;
  *uma_boundary_value = kNaClHelperStatusBoundary;
  return true;
}

pid_t NaClForkDelegate::Fork(const std::string& process_type,
                             const std::vector<std::string>& args,
                             const std::vector<int>& fds,
                             const std::string& channel_id) {
  VLOG(1) << "NaClForkDelegate::Fork";

  // The metrics shared memory handle may or may not be in |fds|, depending on
  // whether the feature flag to pass the handle on startup was enabled in the
  // parent; there should either be kNumPassedFDs or kNumPassedFDs-1 present.
  // TODO(crbug.com/40109064): Only check for kNumPassedFDs once passing the
  // metrics shared memory handle on startup is launched.
  DCHECK(fds.size() == kNumPassedFDs || fds.size() == kNumPassedFDs - 1);

  if (status_ != kNaClHelperSuccess) {
    LOG(ERROR) << "Cannot launch NaCl process: nacl_helper failed to start";
    return -1;
  }

  // First, send a remote fork request.
  base::Pickle write_pickle;
  write_pickle.WriteInt(nacl::kNaClForkRequest);
  write_pickle.WriteString(channel_id);
  write_pickle.WriteInt(base::checked_cast<int>(args.size()));
  for (const std::string& arg : args) {
    write_pickle.WriteString(arg);
  }

  char reply_buf[kNaClMaxIPCMessageLength];
  ssize_t reply_size = 0;
  bool got_reply =
      SendIPCRequestAndReadReply(fd_, fds, write_pickle,
                                 reply_buf, sizeof(reply_buf), &reply_size);
  if (!got_reply) {
    LOG(ERROR) << "Could not perform remote fork.";
    return -1;
  }

  // Now see if the other end managed to fork.
  base::Pickle reply_pickle = base::Pickle::WithUnownedBuffer(
      base::span(reinterpret_cast<uint8_t*>(reply_buf),
                 base::checked_cast<size_t>(reply_size)));
  base::PickleIterator iter(reply_pickle);
  pid_t nacl_child;
  if (!iter.ReadInt(&nacl_child)) {
    LOG(ERROR) << "NaClForkDelegate::Fork: pickle failed";
    return -1;
  }
  VLOG(1) << "nacl_child is " << nacl_child;
  return nacl_child;
}

bool NaClForkDelegate::GetTerminationStatus(pid_t pid, bool known_dead,
                                            base::TerminationStatus* status,
                                            int* exit_code) {
  VLOG(1) << "NaClForkDelegate::GetTerminationStatus";
  DCHECK(status);
  DCHECK(exit_code);

  base::Pickle write_pickle;
  write_pickle.WriteInt(nacl::kNaClGetTerminationStatusRequest);
  write_pickle.WriteInt(pid);
  write_pickle.WriteBool(known_dead);

  const std::vector<int> empty_fds;
  char reply_buf[kNaClMaxIPCMessageLength];
  ssize_t reply_size = 0;
  bool got_reply =
      SendIPCRequestAndReadReply(fd_, empty_fds, write_pickle,
                                 reply_buf, sizeof(reply_buf), &reply_size);
  if (!got_reply) {
    LOG(ERROR) << "Could not perform remote GetTerminationStatus.";
    return false;
  }

  base::Pickle reply_pickle = base::Pickle::WithUnownedBuffer(
      base::span(reinterpret_cast<uint8_t*>(reply_buf),
                 base::checked_cast<size_t>(reply_size)));
  base::PickleIterator iter(reply_pickle);
  int termination_status;
  if (!iter.ReadInt(&termination_status) ||
      termination_status < 0 ||
      termination_status >= base::TERMINATION_STATUS_MAX_ENUM) {
    LOG(ERROR) << "GetTerminationStatus: pickle failed";
    return false;
  }

  int remote_exit_code;
  if (!iter.ReadInt(&remote_exit_code)) {
    LOG(ERROR) << "GetTerminationStatus: pickle failed";
    return false;
  }

  *status = static_cast<base::TerminationStatus>(termination_status);
  *exit_code = remote_exit_code;
  return true;
}

// static
void NaClForkDelegate::AddPassthroughEnvToOptions(
    base::LaunchOptions* options) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string pass_through_string;
  std::vector<std::string> pass_through_vars;
  if (env->GetVar(kNaClEnvPassthrough, &pass_through_string)) {
    pass_through_vars = base::SplitString(
        pass_through_string, std::string(1, kNaClEnvPassthroughDelimiter),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  pass_through_vars.push_back(kNaClExeStderr);
  pass_through_vars.push_back(kNaClExeStdout);
  pass_through_vars.push_back(kNaClVerbosity);
  pass_through_vars.push_back(sandbox::kSandboxEnvironmentApiRequest);
  for (size_t i = 0; i < pass_through_vars.size(); ++i) {
    std::string temp;
    if (env->GetVar(pass_through_vars[i], &temp))
      options->environment[pass_through_vars[i]] = temp;
  }
}

}  // namespace nacl
