// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/libassistant_sandbox_hook.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/services/libassistant/constants.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/ash/services/libassistant/libassistant_loader_impl.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace ash::libassistant {

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
  base::FilePath assistant_path =
      base::FilePath(kAssistantBaseDirPath).AsEndingWithSeparator();
  CHECK(base::CreateDirectory(assistant_path));
  // Save Libassistant logs.
  base::FilePath log_path =
      assistant_path.Append(FILE_PATH_LITERAL("log")).AsEndingWithSeparator();
  CHECK(base::CreateDirectory(log_path));

  // Socket files used for gRPC.
  const bool is_chromeos_device = base::SysInfo::IsRunningOnChromeOS();
  base::FilePath assistant_socket =
      base::FilePath(GetAssistantSocketFileName(is_chromeos_device));
  base::FilePath libassistant_socket =
      base::FilePath(GetLibassistantSocketFileName(is_chromeos_device));
  base::FilePath http_connection_socket =
      base::FilePath(GetHttpConnectionSocketFileName(is_chromeos_device));

  std::vector<BrokerFilePermission> permissions{
      // Required by Libassistant to generate random string.
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadWriteCreateRecursive(assistant_path.value()),
      BrokerFilePermission::ReadWriteCreate(assistant_socket.value()),
      BrokerFilePermission::ReadWriteCreate(libassistant_socket.value()),
      BrokerFilePermission::ReadWriteCreate(http_connection_socket.value()),
  };
  return permissions;
}

}  // namespace

bool LibassistantPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  // Load libassistant DLC before the sandbox initializes.
  LibassistantLoaderImpl::GetInstance()->LoadBlocking(kLibAssistantDlcRootPath);

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(GetLibassistantBrokerCommandSet(),
                               GetLibassistantFilePermissions(), options);

  return true;
}

}  // namespace ash::libassistant
