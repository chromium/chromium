// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/sandbox/screen_ai_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/files/file_util.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace screen_ai {

namespace {

NO_SANITIZE("cfi-icall")
void CallPresandboxInitFunction(void* presandbox_init_function) {
  DCHECK(presandbox_init_function);
  typedef void (*PresandboxInitFn)();
  (*reinterpret_cast<PresandboxInitFn>(presandbox_init_function))();
}

}  // namespace

bool ScreenAIPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  base::FilePath library_path = screen_ai::GetLatestComponentBinaryPath();
  if (library_path.empty()) {
    VLOG(0) << "Screen AI component binary not found.";
  } else {
    void* screen_ai_library = dlopen(library_path.value().c_str(),
                                     RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
    // The library is delivered by the component updater or DLC. If it is not
    // available or has loading or syntax problems, we cannot do anything about
    // them here. The requests to the service will fail later as the library
    // does not exist or does not initialize.
    if (screen_ai_library == nullptr) {
      VLOG(0) << dlerror();
      library_path.clear();
    } else {
      void* presandbox_init = dlsym(screen_ai_library, "PresandboxInit");
      if (presandbox_init == nullptr) {
        VLOG(0) << "PresandboxInit function of Screen AI library not found.";
        library_path.clear();
      } else {
        VLOG(2) << "Screen AI library loaded pre-sandboxing: " << library_path;
        CallPresandboxInitFunction(presandbox_init);
      }
    }
  }

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/proc/cpuinfo"),
      BrokerFilePermission::ReadOnly("/proc/meminfo")};

#if BUILDFLAG(IS_CHROMEOS_ASH)
  permissions.push_back(BrokerFilePermission::ReadOnly("/proc/self/status"));
  permissions.push_back(
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/possible"));
  permissions.push_back(
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/present"));
#endif

  // The models are in the same folder as the library, and the library requires
  // read access for them.
  if (!library_path.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        library_path.DirName().MaybeAsASCII() + base::FilePath::kSeparators));
  }

  instance->StartBrokerProcess(
      MakeBrokerCommandSet({sandbox::syscall_broker::COMMAND_ACCESS,
                            sandbox::syscall_broker::COMMAND_OPEN}),
      permissions, sandbox::policy::SandboxLinux::PreSandboxHook(), options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace screen_ai
