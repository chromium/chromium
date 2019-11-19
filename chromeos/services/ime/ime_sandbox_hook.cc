// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_sandbox_hook.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/buildflag.h"
#include "chromeos/services/ime/constants.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace chromeos {
namespace ime {

namespace {
// Whether IME instance shares a same language data path with each other.
inline constexpr bool CrosImeSharedDataEnabled() {
#if BUILDFLAG(ENABLE_CROS_IME_SHARED_DATA)
  return true;
#else
  return false;
#endif
}

void AddSharedLibraryAndDepsPath(
    std::vector<BrokerFilePermission>* permissions) {
  // Where IME decoder shared library and its dependencies will live.
  static const char* kReadOnlyLibDirs[] =
#if defined(__x86_64__) || defined(__aarch64__)
      {"/usr/lib64", "/lib64"};
#else
      {"/usr/lib", "/lib"};
#endif

  for (const char* dir : kReadOnlyLibDirs) {
    std::string path(dir);
    permissions->push_back(
        BrokerFilePermission::StatOnlyWithIntermediateDirs(path));
    permissions->push_back(BrokerFilePermission::ReadOnlyRecursive(path + "/"));
  }
}

void AddBundleFolder(std::vector<BrokerFilePermission>* permissions) {
  base::FilePath bundle_dir =
      base::FilePath(kBundledInputMethodsDirPath).AsEndingWithSeparator();
  permissions->push_back(
      BrokerFilePermission::ReadOnlyRecursive(bundle_dir.value()));
}

void AddSharedDataFolderIfEnabled(
    std::vector<BrokerFilePermission>* permissions) {
  if (!CrosImeSharedDataEnabled())
    return;

  // Without access to shared home folder, IME servcie will download all
  // missing dictionaries to `kUserInputMethodsDirPath` of the current user.
  base::FilePath shared_path =
      base::FilePath(kSharedInputMethodsDirPath).AsEndingWithSeparator();
  if (base::CreateDirectory(shared_path)) {
    permissions->push_back(
        BrokerFilePermission::ReadWriteCreateRecursive(shared_path.value()));
  }
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
  // These 2 paths are needed before creating IME service.
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu")};

  AddSharedLibraryAndDepsPath(&permissions);
  AddBundleFolder(&permissions);
  AddUserDataFolder(&permissions);
  AddSharedDataFolderIfEnabled(&permissions);
  return permissions;
}

}  // namespace

bool ImePreSandboxHook(service_manager::SandboxLinux::Options options) {
  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_MKDIR,
                                   sandbox::syscall_broker::COMMAND_STAT,
                                   sandbox::syscall_broker::COMMAND_STAT64,
                                   sandbox::syscall_broker::COMMAND_RENAME,
                                   sandbox::syscall_broker::COMMAND_UNLINK,
                               }),
                               GetImeFilePermissions(),
                               service_manager::SandboxLinux::PreSandboxHook(),
                               options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace ime
}  // namespace chromeos
