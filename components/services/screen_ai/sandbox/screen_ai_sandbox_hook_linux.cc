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
  // TODO(crbug.com/1278249): Replace this with DCHECK after library is updated
  // to 112.0. OCR function will not work without presandbox init but main
  // content extraction does not require it.
  if (!presandbox_init_function) {
    VLOG(0) << "Screen AI library is outdated. Current version does not "
               "support OCR.";
    return;
  }

  typedef void (*PresandboxInitFn)();
  (*reinterpret_cast<PresandboxInitFn>(presandbox_init_function))();
}

}  // namespace

bool ScreenAIPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  base::FilePath library_path = screen_ai::GetLatestComponentBinaryPath();
  if (library_path.empty()) {
    VLOG(1) << "Screen AI component binary not found.";
  } else {
    void* screen_ai_library = dlopen(library_path.value().c_str(),
                                     RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
    // The library is delivered by the component updater. If it is not available
    // we cannot do anything about it here. The requests to the service will
    // fail later as the library does not exist.
    if (!screen_ai_library) {
      VLOG(1) << dlerror();
      library_path.clear();
    } else {
      VLOG(2) << "Screen AI library loaded pre-sandboxing:" << library_path;
      CallPresandboxInitFunction(dlsym(screen_ai_library, "PresandboxInit"));
    }
  }

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/proc/cpuinfo"),
      BrokerFilePermission::ReadOnly("/proc/meminfo")};

#if BUILDFLAG(IS_CHROMEOS_ASH)
  permissions.push_back(BrokerFilePermission::ReadOnly("/proc/self/status"));
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
