// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/sandbox_linux/nacl_sandbox_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/sandbox_linux/nacl_bpf_sandbox_linux.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/resource_limits.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "sandbox/policy/switches.h"

namespace nacl {

namespace {

// This is a simplistic check of whether we are sandboxed.
bool IsSandboxed() {
  int proc_fd = open("/proc/self/exe", O_RDONLY);
  if (proc_fd >= 0) {
    PCHECK(0 == IGNORE_EINTR(close(proc_fd)));
    return false;
  }
  return true;
}

bool MaybeSetProcessNonDumpable() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          sandbox::policy::switches::kAllowSandboxDebugging)) {
    return true;
  }

  if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0) {
    PLOG(ERROR) << "Failed to set non-dumpable flag";
    return false;
  }

  return prctl(PR_GET_DUMPABLE) == 0;
}

void RestrictAddressSpaceUsage() {
  // Sanitizers need to reserve huge chunks of the address space.
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) && \
    !defined(THREAD_SANITIZER)
  // Add a limit to the brk() heap that would prevent allocations that can't be
  // indexed by an int. This helps working around typical security bugs.
  // This could almost certainly be set to zero. GLibc's allocator and others
  // would fall-back to mmap if brk() fails.
  const rlim_t kNewDataSegmentMaxSize = std::numeric_limits<int>::max();
  CHECK_EQ(0,
           sandbox::ResourceLimits::Lower(RLIMIT_DATA, kNewDataSegmentMaxSize));

#if defined(ARCH_CPU_64_BITS)
  // NaCl's x86-64 sandbox allocated 88GB address of space during startup:
  // - The main sandbox is 4GB
  // - There are two guard regions of 40GB each.
  // - 4GB are allocated extra to have a 4GB-aligned address.
  // See https://crbug.com/455839
  //
  // Set the limit to 128 GB and have some margin.
  const rlim_t kNewAddressSpaceLimit = 1UL << 37;
#else
  // Some architectures such as X86 allow 32 bits processes to switch to 64
  // bits when running under 64 bits kernels. Set a limit in case this happens.
  const rlim_t kNewAddressSpaceLimit = std::numeric_limits<uint32_t>::max();
#endif
  CHECK_EQ(0, sandbox::ResourceLimits::Lower(RLIMIT_AS, kNewAddressSpaceLimit));
#endif
}

}  // namespace

NaClSandbox::NaClSandbox()
    : layer_one_enabled_(false),
      layer_one_sealed_(false),
      layer_two_enabled_(false),
      proc_fd_(-1),
      setuid_sandbox_client_(sandbox::SetuidSandboxClient::Create()) {
  proc_fd_.reset(
      HANDLE_EINTR(open("/proc", O_DIRECTORY | O_RDONLY | O_CLOEXEC)));
  PCHECK(proc_fd_.is_valid());
}

NaClSandbox::~NaClSandbox() {
}

bool NaClSandbox::IsSingleThreaded() {
  CHECK(proc_fd_.is_valid());
  return sandbox::ThreadHelpers::IsSingleThreaded(proc_fd_.get());
}

bool NaClSandbox::HasOpenDirectory() {
  CHECK(proc_fd_.is_valid());
  return sandbox::ProcUtil::HasOpenDirectory(proc_fd_.get());
}

void NaClSandbox::InitializeLayerOneSandbox() {
  // Check that IsSandboxed() works. We should not be sandboxed at this point.
  CHECK(!IsSandboxed()) << "Unexpectedly sandboxed!";

  // Open /dev/urandom while we can. This enables `base::RandBytes` to work. We
  // don't need to store the resulting file descriptor; it's a singleton and
  // subsequent calls to `GetUrandomFD` will return it.
  CHECK_GE(base::GetUrandomFD(), 0);

  if (setuid_sandbox_client_->IsSuidSandboxChild()) {
    setuid_sandbox_client_->CloseDummyFile();

    // Make sure that no directory file descriptor is open, as it would bypass
    // the setuid sandbox model.
    CHECK(!HasOpenDirectory());

    // Get sandboxed.
    CHECK(setuid_sandbox_client_->ChrootMe());
    CHECK(MaybeSetProcessNonDumpable());
    CHECK(IsSandboxed());
    layer_one_enabled_ = true;
  } else if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
    CHECK(sandbox::Credentials::MoveToNewUserNS());
    CHECK(sandbox::Credentials::DropFileSystemAccess(proc_fd_.get()));

    // We do not drop CAP_SYS_ADMIN because we need it to place each child
    // process in its own PID namespace later on.
    std::vector<sandbox::Credentials::Capability> caps;
    caps.push_back(sandbox::Credentials::Capability::SYS_ADMIN);
    CHECK(sandbox::Credentials::SetCapabilities(proc_fd_.get(), caps));

    CHECK(IsSandboxed());
    layer_one_enabled_ = true;
  }
}

void NaClSandbox::CheckForExpectedNumberOfOpenFds() {
  // We expect to have the following FDs open:
  //  1-3) stdin, stdout, stderr.
  //  4) The /dev/urandom FD used by base::GetUrandomFD().
  //  5) A dummy pipe FD used to overwrite kSandboxIPCChannel.
  //  6) The socket for the Chrome IPC channel that's connected to the
  //     browser process, kPrimaryIPCChannel.
  // We also have an fd for /proc (proc_fd_), but CountOpenFds excludes this.
  //
  // This sanity check ensures that dynamically loaded libraries don't
  // leave any FDs open before we enable the sandbox.
  int expected_num_fds = 6;
  if (setuid_sandbox_client_->IsSuidSandboxChild()) {
    // When using the setuid sandbox, there is one additional socket used for
    // ChrootMe(). After ChrootMe(), it is no longer connected to anything.
    ++expected_num_fds;
  }

  CHECK_EQ(expected_num_fds, sandbox::ProcUtil::CountOpenFds(proc_fd_.get()));
}

void NaClSandbox::InitializeLayerTwoSandbox() {
  // seccomp-bpf only applies to the current thread, so it's critical to only
  // have a single thread running here.
  DCHECK(!layer_one_sealed_);
  CHECK(IsSingleThreaded());
  CheckForExpectedNumberOfOpenFds();

  RestrictAddressSpaceUsage();

  // Pass proc_fd_ ownership to the BPF sandbox, which guarantees it will
  // be closed. There is no point in keeping it around since the BPF policy
  // will prevent its usage.
  layer_two_enabled_ = nacl::InitializeBPFSandbox(std::move(proc_fd_));
}

void NaClSandbox::SealLayerOneSandbox() {
  if (proc_fd_.is_valid() && !layer_two_enabled_) {
    // If nothing prevents us, check that there is no superfluous directory
    // open.
    CHECK(!HasOpenDirectory());
  }
  proc_fd_.reset();
  layer_one_sealed_ = true;
}

void NaClSandbox::CheckSandboxingStateWithPolicy() {
  LOG_IF(ERROR, !layer_one_enabled_ || !layer_one_sealed_)
      << "The SUID sandbox is not engaged for NaCl: this is dangerous.";
  LOG_IF(ERROR, !layer_two_enabled_)
      << "The seccomp-bpf sandbox is not engaged for NaCl: this is dangerous.";
}

}  // namespace nacl
