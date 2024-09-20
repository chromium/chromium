// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/ime_sandbox_hook.h"

#include <dlfcn.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromeos/ash/services/ime/constants.h"
#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace ash {
namespace ime {

namespace {
void AddBundleFolder(std::vector<BrokerFilePermission>* permissions) {
  base::FilePath bundle_dir =
      base::FilePath(kBundledInputMethodsDirPath).AsEndingWithSeparator();
  permissions->push_back(
      BrokerFilePermission::ReadOnlyRecursive(bundle_dir.value()));
}

void AddUserDataFolder(std::vector<BrokerFilePermission>* permissions) {
  // When failed to access user profile folder, decoder still can work, but
  // user dictionary can not be saved.
  base::FilePath user_path =
      base::FilePath(kUserInputMethodsDirPath).AsEndingWithSeparator();
  bool success = base::CreateDirectory(user_path);
  if (!success) {
    LOG(WARNING) << "Unable to create IME folder under user profile folder";
    return;
  }
  // Push this path, otherwise process will crash directly when IME decoder
  // tries to access this folder.
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateRecursive(user_path.value()));
}

std::vector<BrokerFilePermission> GetImeFilePermissions() {
  // These paths are needed before creating IME service.
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu"),
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/possible")};

  AddBundleFolder(&permissions);
  AddUserDataFolder(&permissions);
  return permissions;
}

}  // namespace

bool ImePreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_MKDIR,
                                   sandbox::syscall_broker::COMMAND_STAT,
                                   sandbox::syscall_broker::COMMAND_STAT64,
                                   sandbox::syscall_broker::COMMAND_RENAME,
                                   sandbox::syscall_broker::COMMAND_UNLINK,
                               }),
                               GetImeFilePermissions(), options);

  // Try to load IME decoder shared library.
  // TODO(crbug.com/40185212): This is not ideal, as it means rule-based
  // input methods will unnecessarily load the IME decoder shared library.
  // Either remove this line, or use a separate sandbox for rule-based.
  ImeSharedLibraryWrapperImpl::GetInstance()->MaybeLoadThenReturnEntryPoints();
  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace ime
}  // namespace ash
