// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// A mini-zygote specifically for Native Client.

#include "components/nacl/loader/nacl_helper_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_shared_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nacl_listener.h"
#include "components/nacl/loader/sandbox_linux/nacl_sandbox_linux.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/zygote/send_zygote_child_ping_linux.h"
#include "content/public/common/zygote/zygote_fork_delegate_linux.h"
#include "mojo/core/embedder/embedder.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace {

struct NaClLoaderSystemInfo {
  size_t prereserved_sandbox_size;
  long number_of_cores;
};

#if BUILDFLAG(IS_CHROMEOS)
std::string GetCommandLineFeatureFlagChoice(
    const base::CommandLine* command_line,
    std::string feature_flag) {
  std::string encoded =
      command_line->GetSwitchValueNative(chromeos::switches::kFeatureFlags);
  if (encoded.empty()) {
    return "";
  }

  auto flags_list = base::JSONReader::Read(encoded);
  if (!flags_list) {
    LOG(WARNING) << "Failed to parse feature flags configuration";
    return "";
  }

  for (const auto& flag : flags_list.value().GetList()) {
    if (!flag.is_string())
      continue;
    std::string flag_string = flag.GetString();
    if (flag_string.rfind(feature_flag) != std::string::npos)
      // For option x, this has the form "feature-flag-name@x". Return "x".
      return flag_string.substr(feature_flag.size() + 1);
  }
  return "";
}

void AddVerboseLoggingInNaclSwitch(base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kVerboseLoggingInNacl))
    // Flag is already present, nothing to do here.
    return;

  std::string option = GetCommandLineFeatureFlagChoice(
      command_line, switches::kVerboseLoggingInNacl);

  // This needs to be kept in sync with the order of choices for
  // kVerboseLoggingInNacl in chrome/browser/about_flags.cc
  if (option == "")
    return;
  if (option == "1")
    return command_line->AppendSwitchASCII(
        switches::kVerboseLoggingInNacl,
        switches::kVerboseLoggingInNaclChoiceLow);
  if (option == "2")
    return command_line->AppendSwitchASCII(
        switches::kVerboseLoggingInNacl,
        switches::kVerboseLoggingInNaclChoiceMedium);
  if (option == "3")
    return command_line->AppendSwitchASCII(
        switches::kVerboseLoggingInNacl,
        switches::kVerboseLoggingInNaclChoiceHigh);
  if (option == "4")
    return command_line->AppendSwitchASCII(
        switches::kVerboseLoggingInNacl,
        switches::kVerboseLoggingInNaclChoiceHighest);
  if (option == "5")
    return command_line->AppendSwitchASCII(
        switches::kVerboseLoggingInNacl,
        switches::kVerboseLoggingInNaclChoiceDisabled);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// The child must mimic the behavior of zygote_main_linux.cc on the child
// side of the fork. See zygote_main_linux.cc:HandleForkRequest from
//   if (!child) {
void BecomeNaClLoader(base::ScopedFD browser_fd,
                      const NaClLoaderSystemInfo& system_info,
                      nacl::NaClSandbox* nacl_sandbox,
                      const std::vector<std::string>& args) {
  DCHECK(nacl_sandbox);
  VLOG(1) << "NaCl loader: setting up IPC descriptor";
  // Close or shutdown IPC channels that we don't need anymore.
  PCHECK(0 == IGNORE_EINTR(close(kNaClZygoteDescriptor)));

  // Append any passed switches to the forked loader's command line. This is
  // necessary to get e.g. any field trial handle and feature overrides from
  // whomever initiated the this fork request.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendArguments(base::CommandLine::FromArgvWithoutProgram(args),
                               /*include_program=*/false);
  if (command_line.HasSwitch(switches::kVerboseLoggingInNacl)) {
    base::Environment::Create()->SetVar(
        "NACLVERBOSITY",
        command_line.GetSwitchValueASCII(switches::kVerboseLoggingInNacl));
  }

  // Always ignore SIGPIPE, for consistency with other Chrome processes and
  // because some IPC code, such as sync_socket_posix.cc, requires this.
  // We do this before seccomp-bpf is initialized.
  PCHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);

  base::HistogramSharedMemory::InitFromLaunchParameters(command_line);

  base::FieldTrialList field_trial_list;
  base::FieldTrialList::CreateTrialsInChildProcess(command_line);
  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrialList::ApplyFeatureOverridesInChildProcess(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  // Finish layer-1 sandbox initialization and initialize the layer-2 sandbox.
  CHECK(!nacl_sandbox->HasOpenDirectory());
  nacl_sandbox->InitializeLayerTwoSandbox();
  nacl_sandbox->SealLayerOneSandbox();
  nacl_sandbox->CheckSandboxingStateWithPolicy();

  base::GlobalDescriptors::GetInstance()->Set(kMojoIPCChannel,
                                              browser_fd.release());

  // The Mojo EDK must be initialized before using IPC.
  mojo::core::InitFeatures();
  mojo::core::Init();

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  NaClListener listener;
  listener.set_prereserved_sandbox_size(system_info.prereserved_sandbox_size);
  listener.set_number_of_cores(system_info.number_of_cores);
  listener.Listen();

  _exit(0);
}

// Start the NaCl loader in a child created by the NaCl loader Zygote.
void ChildNaClLoaderInit(std::vector<base::ScopedFD> child_fds,
                         const NaClLoaderSystemInfo& system_info,
                         nacl::NaClSandbox* nacl_sandbox,
                         const std::string& channel_id,
                         const std::vector<std::string>& args) {
  DCHECK(child_fds.size() >
         std::max(content::ZygoteForkDelegate::kPIDOracleFDIndex,
                  content::ZygoteForkDelegate::kBrowserFDIndex));

  // Ping the PID oracle socket.
  CHECK(content::SendZygoteChildPing(
      child_fds[content::ZygoteForkDelegate::kPIDOracleFDIndex].get()));

  // Stash the field trial descriptor in GlobalDescriptors so FieldTrialList
  // can be initialized later. See BecomeNaClLoader().
  base::GlobalDescriptors::GetInstance()->Set(
      kFieldTrialDescriptor,
      child_fds[content::ZygoteForkDelegate::kFieldTrialFDIndex].release());

  // Stash the histogram descriptor in GlobalDescriptors so the histogram
  // allocator can be initialized later. See BecomeNaClLoader().
  // TODO(crbug.com/40109064): Always update mapping once metrics shared memory
  // region is always passed on startup.
  if (child_fds.size() > content::ZygoteForkDelegate::kHistogramFDIndex &&
      child_fds[content::ZygoteForkDelegate::kHistogramFDIndex].is_valid()) {
    base::GlobalDescriptors::GetInstance()->Set(
        kHistogramSharedMemoryDescriptor,
        child_fds[content::ZygoteForkDelegate::kHistogramFDIndex].release());
  }

  // Save the browser socket and close the rest.
  base::ScopedFD browser_fd(
      std::move(child_fds[content::ZygoteForkDelegate::kBrowserFDIndex]));
  child_fds.clear();

  BecomeNaClLoader(std::move(browser_fd), system_info, nacl_sandbox, args);
  _exit(1);
}

// Handle a fork request from the Zygote.
// Some of this code was lifted from
// content/browser/zygote_main_linux.cc:ForkWithRealPid()
bool HandleForkRequest(std::vector<base::ScopedFD> child_fds,
                       const NaClLoaderSystemInfo& system_info,
                       nacl::NaClSandbox* nacl_sandbox,
                       base::PickleIterator* input_iter,
                       base::Pickle* output_pickle) {
  std::string channel_id;
  if (!input_iter->ReadString(&channel_id)) {
    LOG(ERROR) << "Could not read channel_id string";
    return false;
  }

  // Read the args passed by the launcher and prepare to forward them to our own
  // forked child.
  int argc;
  if (!input_iter->ReadInt(&argc) || argc < 0) {
    LOG(ERROR) << "nacl_helper: Invalid argument list";
    return false;
  }
  std::vector<std::string> args(static_cast<size_t>(argc));
  for (std::string& arg : args) {
    if (!input_iter->ReadString(&arg)) {
      LOG(ERROR) << "nacl_helper: Invalid argument list";
      return false;
    }
  }

  // |child_fds| should contain either kNumPassedFDs or kNumPassedFDs-1 file
  // descriptors.. The actual size of |child_fds| depends on whether or not the
  // metrics shared memory region is being passed on startup.
  // TODO(crbug.com/40109064): Expect a fixed size once passing the metrics
  // shared memory region on startup has been launched.
  if (child_fds.size() != content::ZygoteForkDelegate::kNumPassedFDs &&
      child_fds.size() != content::ZygoteForkDelegate::kNumPassedFDs - 1) {
    LOG(ERROR) << "nacl_helper: unexpected number of fds, got "
        << child_fds.size();
    return false;
  }

  VLOG(1) << "nacl_helper: forking";
  pid_t child_pid;
  if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
    child_pid = sandbox::NamespaceSandbox::ForkInNewPidNamespace(
        /*drop_capabilities_in_child=*/true);
  } else {
    child_pid = sandbox::Credentials::ForkAndDropCapabilitiesInChild();
  }

  if (child_pid < 0) {
    PLOG(ERROR) << "*** fork() failed.";
  }

  if (child_pid == 0) {
    ChildNaClLoaderInit(std::move(child_fds), system_info, nacl_sandbox,
                        channel_id, args);
    NOTREACHED_IN_MIGRATION();
  }

  // I am the parent.
  // First, close the dummy_fd so the sandbox won't find me when
  // looking for the child's pid in /proc. Also close other fds.
  child_fds.clear();
  VLOG(1) << "nacl_helper: child_pid is " << child_pid;

  // Now send child_pid (eventually -1 if fork failed) to the Chrome Zygote.
  output_pickle->WriteInt(child_pid);
  return true;
}

bool HandleGetTerminationStatusRequest(base::PickleIterator* input_iter,
                                       base::Pickle* output_pickle) {
  pid_t child_to_wait;
  if (!input_iter->ReadInt(&child_to_wait)) {
    LOG(ERROR) << "Could not read pid to wait for";
    return false;
  }

  bool known_dead;
  if (!input_iter->ReadBool(&known_dead)) {
    LOG(ERROR) << "Could not read known_dead status";
    return false;
  }
  // TODO(jln): With NaCl, known_dead seems to never be set to true (unless
  // called from the Zygote's kZygoteCommandReap command). This means that we
  // will sometimes detect the process as still running when it's not. Fix
  // this!

  int exit_code;
  base::TerminationStatus status;
  if (known_dead)
    status = base::GetKnownDeadTerminationStatus(child_to_wait, &exit_code);
  else
    status = base::GetTerminationStatus(child_to_wait, &exit_code);
  output_pickle->WriteInt(static_cast<int>(status));
  output_pickle->WriteInt(exit_code);
  return true;
}

// Honor a command |command_type|. Eventual command parameters are
// available in |input_iter| and eventual file descriptors attached to
// the command are in |attached_fds|.
// Reply to the command on |reply_fds|.
bool HonorRequestAndReply(int reply_fd,
                          int command_type,
                          std::vector<base::ScopedFD> attached_fds,
                          const NaClLoaderSystemInfo& system_info,
                          nacl::NaClSandbox* nacl_sandbox,
                          base::PickleIterator* input_iter) {
  base::Pickle write_pickle;
  bool have_to_reply = false;
  // Commands must write anything to send back to |write_pickle|.
  switch (command_type) {
    case nacl::kNaClForkRequest:
      have_to_reply =
          HandleForkRequest(std::move(attached_fds), system_info, nacl_sandbox,
                            input_iter, &write_pickle);
      break;
    case nacl::kNaClGetTerminationStatusRequest:
      have_to_reply =
          HandleGetTerminationStatusRequest(input_iter, &write_pickle);
      break;
    default:
      LOG(ERROR) << "Unsupported command from Zygote";
      return false;
  }
  if (!have_to_reply)
    return false;
  const std::vector<int> empty;  // We never send file descriptors back.
  if (!base::UnixDomainSocket::SendMsg(reply_fd, write_pickle.data(),
                                       write_pickle.size(), empty)) {
    LOG(ERROR) << "*** send() to zygote failed";
    return false;
  }
  return true;
}

// Read a request from the Zygote from |zygote_ipc_fd| and handle it.
// Die on EOF from |zygote_ipc_fd|.
bool HandleZygoteRequest(int zygote_ipc_fd,
                         const NaClLoaderSystemInfo& system_info,
                         nacl::NaClSandbox* nacl_sandbox) {
  std::vector<base::ScopedFD> fds;
  uint8_t buf[kNaClMaxIPCMessageLength];
  const ssize_t msglen = base::UnixDomainSocket::RecvMsg(zygote_ipc_fd,
      &buf, sizeof(buf), &fds);
  // If the Zygote has started handling requests, we should be sandboxed via
  // the setuid sandbox.
  if (!nacl_sandbox->layer_one_enabled()) {
    LOG(ERROR) << "NaCl helper process running without a sandbox!\n"
      << "Most likely you need to configure your SUID sandbox "
      << "correctly";
  }
  if (msglen == 0 || (msglen == -1 && errno == ECONNRESET)) {
    // EOF from the browser. Goodbye!
    _exit(0);
  }
  if (msglen < 0) {
    PLOG(ERROR) << "nacl_helper: receive from zygote failed";
    return false;
  }

  base::Pickle read_pickle = base::Pickle::WithUnownedBuffer(
      base::span(buf, base::checked_cast<size_t>(msglen)));
  base::PickleIterator read_iter(read_pickle);
  int command_type;
  if (!read_iter.ReadInt(&command_type)) {
    LOG(ERROR) << "Unable to read command from Zygote";
    return false;
  }
  return HonorRequestAndReply(zygote_ipc_fd, command_type, std::move(fds),
                              system_info, nacl_sandbox, &read_iter);
}

static const char kNaClHelperReservedAtZero[] = "reserved_at_zero";
static const char kNaClHelperRDebug[] = "r_debug";

// Since we were started by nacl_helper_bootstrap rather than in the
// usual way, the debugger cannot figure out where our executable
// or the dynamic linker or the shared libraries are in memory,
// so it won't find any symbols.  But we can fake it out to find us.
//
// The zygote passes --r_debug=0xXXXXXXXXXXXXXXXX.
// nacl_helper_bootstrap replaces the Xs with the address of its _r_debug
// structure.  The debugger will look for that symbol by name to
// discover the addresses of key dynamic linker data structures.
// Since all it knows about is the original main executable, which
// is the bootstrap program, it finds the symbol defined there.  The
// dynamic linker's structure is somewhere else, but it is filled in
// after initialization.  The parts that really matter to the
// debugger never change.  So we just copy the contents of the
// dynamic linker's structure into the address provided by the option.
// Hereafter, if someone attaches a debugger (or examines a core dump),
// the debugger will find all the symbols in the normal way.
static void CheckRDebug(char* argv0) {
  std::string r_debug_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kNaClHelperRDebug);
  if (!r_debug_switch_value.empty()) {
    char* endp;
    uintptr_t r_debug_addr = strtoul(r_debug_switch_value.c_str(), &endp, 0);
    if (r_debug_addr != 0 && *endp == '\0') {
      r_debug* bootstrap_r_debug = reinterpret_cast<r_debug*>(r_debug_addr);
      *bootstrap_r_debug = _r_debug;

      // Since the main executable (the bootstrap program) does not
      // have a dynamic section, the debugger will not skip the
      // first element of the link_map list as it usually would for
      // an executable or PIE that was loaded normally.  But the
      // dynamic linker has set l_name for the PIE to "" as is
      // normal for the main executable.  So the debugger doesn't
      // know which file it is.  Fill in the actual file name, which
      // came in as our argv[0].
      link_map* l = _r_debug.r_map;
      if (l->l_name[0] == '\0')
        l->l_name = argv0;
    }
  }
}

// The zygote passes --reserved_at_zero=0xXXXXXXXXXXXXXXXX.
// nacl_helper_bootstrap replaces the Xs with the amount of prereserved
// sandbox memory.
//
// CheckReservedAtZero parses the value of the argument reserved_at_zero
// and returns the amount of prereserved sandbox memory.
static size_t CheckReservedAtZero() {
  size_t prereserved_sandbox_size = 0;
  std::string reserved_at_zero_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kNaClHelperReservedAtZero);
  if (!reserved_at_zero_switch_value.empty()) {
    char* endp;
    prereserved_sandbox_size =
        strtoul(reserved_at_zero_switch_value.c_str(), &endp, 0);
    if (*endp != '\0')
      LOG(ERROR) << "Could not parse reserved_at_zero argument value of "
                 << reserved_at_zero_switch_value;
  }
  return prereserved_sandbox_size;
}

}  // namespace

#if defined(ADDRESS_SANITIZER)
// Do not install the SIGSEGV handler in ASan. This should make the NaCl
// platform qualification test pass.
// detect_odr_violation=0: http://crbug.com/376306
extern const char* kAsanDefaultOptionsNaCl;
const char* kAsanDefaultOptionsNaCl = "handle_segv=0:detect_odr_violation=0";
#endif

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;
  base::RandUint64();  // acquire /dev/urandom fd before sandbox is raised

  const NaClLoaderSystemInfo system_info = {CheckReservedAtZero(),
                                            sysconf(_SC_NPROCESSORS_ONLN)};

  CheckRDebug(argv[0]);

#if BUILDFLAG(IS_CHROMEOS)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  AddVerboseLoggingInNaclSwitch(command_line);
  if (command_line->HasSwitch(switches::kVerboseLoggingInNacl)) {
    if (!freopen("/tmp/nacl.log", "a", stderr))
      LOG(WARNING) << "Could not open /tmp/nacl.log";
  }
#endif

  std::unique_ptr<nacl::NaClSandbox> nacl_sandbox(new nacl::NaClSandbox);
  // Make sure that the early initialization did not start any spurious
  // threads.
#if !defined(THREAD_SANITIZER)
  CHECK(nacl_sandbox->IsSingleThreaded());
#endif

  const bool is_init_process = 1 == getpid();
  nacl_sandbox->InitializeLayerOneSandbox();
  CHECK_EQ(is_init_process, nacl_sandbox->layer_one_enabled());

  const std::vector<int> empty;
  // Send the zygote a message to let it know we are ready to help
  if (!base::UnixDomainSocket::SendMsg(kNaClZygoteDescriptor,
                                       kNaClHelperStartupAck,
                                       sizeof(kNaClHelperStartupAck), empty)) {
    LOG(ERROR) << "*** send() to zygote failed";
  }

  // Now handle requests from the Zygote.
  while (true) {
    bool request_handled = HandleZygoteRequest(
        kNaClZygoteDescriptor, system_info, nacl_sandbox.get());
    // Do not turn this into a CHECK() without thinking about robustness
    // against malicious IPC requests.
    DCHECK(request_handled);
  }
  NOTREACHED_IN_MIGRATION();
}
