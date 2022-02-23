// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/sandbox/screen_ai_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/files/file_util.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace screen_ai {

bool ScreenAIPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  // TODO(https://crbug.com/1278249): Add a common getter function for the
  // library file path.
  const base::FilePath library_path =
      base::FilePath(FILE_PATH_LITERAL("/"))
          .Append(FILE_PATH_LITERAL("lib"))
          .Append(FILE_PATH_LITERAL("libchrome_screen_ai.so"));

  void* screen_ai_library = dlopen(library_path.value().c_str(),
                                   RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
  // TODO(https://crbug.com/1278249): Consider handling differently when library
  // is downloaded using component updater and feature is enabled by default.
  if (!screen_ai_library)
    VLOG(1) << dlerror();

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/proc/meminfo")};

  instance->StartBrokerProcess(
      MakeBrokerCommandSet({sandbox::syscall_broker::COMMAND_ACCESS,
                            sandbox::syscall_broker::COMMAND_OPEN}),
      permissions, sandbox::policy::SandboxLinux::PreSandboxHook(), options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace screen_ai
