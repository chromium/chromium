// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/libassistant_sandbox_hook.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "chromeos/services/libassistant/constants.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace chromeos {
namespace libassistant {

namespace {

sandbox::syscall_broker::BrokerCommandSet GetLibassistantBrokerCommandSet() {
  return MakeBrokerCommandSet({
      sandbox::syscall_broker::COMMAND_ACCESS,
      sandbox::syscall_broker::COMMAND_MKDIR,
      sandbox::syscall_broker::COMMAND_OPEN,
      sandbox::syscall_broker::COMMAND_RENAME,
      sandbox::syscall_broker::COMMAND_STAT,
      sandbox::syscall_broker::COMMAND_STAT64,
  });
}

std::vector<BrokerFilePermission> GetLibassistantFilePermissions() {
  base::FilePath assistant_path;
  if (base::SysInfo::IsRunningOnChromeOS()) {
    assistant_path =
        base::FilePath(kAssistantBaseDirPath).AsEndingWithSeparator();
  } else {
    assistant_path =
        base::FilePath(kAssistantTempBaseDirPath).AsEndingWithSeparator();
  }
  CHECK(base::CreateDirectory(assistant_path));

  // Save Libassistant logs.
  base::FilePath log_path =
      assistant_path.Append(FILE_PATH_LITERAL("log")).AsEndingWithSeparator();
  CHECK(base::CreateDirectory(log_path));

  std::vector<BrokerFilePermission> permissions{
      // Required by Libassistant to generate random string.
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadWriteCreateRecursive(assistant_path.value()),
  };
  return permissions;
}

}  // namespace

bool LibassistantPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  instance->StartBrokerProcess(
      GetLibassistantBrokerCommandSet(), GetLibassistantFilePermissions(),
      sandbox::policy::SandboxLinux::PreSandboxHook(), options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace libassistant
}  // namespace chromeos
