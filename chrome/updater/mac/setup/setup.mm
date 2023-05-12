// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/posix/setup.h"

#import <ServiceManagement/ServiceManagement.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/apple/bundle_locations.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/mac/setup/keystone.h"
#include "chrome/updater/mac/setup/wake_task.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#import "chrome/updater/util/mac_util.h"
#import "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace updater {
namespace {

bool CopyBundle(UpdaterScope scope) {
  absl::optional<base::FilePath> base_install_dir = GetInstallDirectory(scope);
  absl::optional<base::FilePath> versioned_install_dir =
      GetVersionedInstallDirectory(scope);
  if (!base_install_dir || !versioned_install_dir) {
    LOG(ERROR) << "Failed to get install directory.";
    return false;
  }
  if (!base::PathExists(*versioned_install_dir)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(*versioned_install_dir, &error)) {
      LOG(ERROR) << "Failed to create '"
                 << versioned_install_dir->value().c_str()
                 << "' directory: " << base::File::ErrorToString(error);
      return false;
    }
  }

  // For system installs, set file permissions to be drwxr-xr-x
  if (IsSystemInstall(scope)) {
    constexpr int kPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                     base::FILE_PERMISSION_READ_BY_GROUP |
                                     base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                     base::FILE_PERMISSION_READ_BY_OTHERS |
                                     base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
    if (!base::SetPosixFilePermissions(base_install_dir->DirName(),
                                       kPermissionsMask) ||
        !base::SetPosixFilePermissions(*base_install_dir, kPermissionsMask) ||
        !base::SetPosixFilePermissions(*versioned_install_dir,
                                       kPermissionsMask)) {
      LOG(ERROR) << "Failed to set permissions to drwxr-xr-x at "
                 << versioned_install_dir->value();
      return false;
    }
  }

  if (!CopyDir(base::apple::OuterBundlePath(), *versioned_install_dir,
               scope == UpdaterScope::kSystem)) {
    LOG(ERROR) << "Copying app to '" << versioned_install_dir->value().c_str()
               << "' failed";
    return false;
  }

  return true;
}

bool CreateWakeLaunchdJobPlist(UpdaterScope scope) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  NSDictionary* plist = CreateWakeLaunchdPlist(scope);
  if (!plist) {
    return false;
  }
  return EnsureWakeLaunchItemPresence(scope, plist);
}

void CleanAfterInstallFailure(UpdaterScope scope) {
  // If install fails at any point, attempt to clean the install.
  DeleteCandidateInstallFolder(scope);
}

int DoSetup(UpdaterScope scope) {
  if (!CopyBundle(scope)) {
    return kErrorFailedToCopyBundle;
  }

  // Quarantine attribute needs to be removed here as the copied bundle might be
  // given com.apple.quarantine attribute, and the server is attempted to be
  // launched below, Gatekeeper could prompt the user.
  const absl::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  if (!install_dir) {
    return kErrorFailedToGetInstallDir;
  }
  if (!RemoveQuarantineAttributes(*install_dir)) {
    VLOG(1) << "Couldn't remove quarantine bits for updater. This will likely "
               "cause Gatekeeper to show a prompt to the user.";
  }

  // If there is no Current symlink, create one now.
  base::FilePath current_symlink = install_dir->Append("Current");
  if (!base::PathExists(current_symlink)) {
    if (base::DeleteFile(current_symlink) &&
        symlink(kUpdaterVersion, current_symlink.value().c_str())) {
      return kErrorFailedToLinkLauncher;
    }
  }

  if (!CreateWakeLaunchdJobPlist(scope)) {
    return kErrorFailedToCreateWakeLaunchdJobPlist;
  }

  return kErrorOk;
}

}  // namespace

int Setup(UpdaterScope scope) {
  int error = DoSetup(scope);
  if (error)
    CleanAfterInstallFailure(scope);
  return error;
}

int PromoteCandidate(UpdaterScope scope) {
  const absl::optional<base::FilePath> updater_executable_path =
      GetUpdaterExecutablePath(scope);
  const absl::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  const absl::optional<base::FilePath> bundle_path =
      GetUpdaterAppBundlePath(scope);
  if (!updater_executable_path || !install_dir || !bundle_path) {
    return kErrorFailedToGetVersionedInstallDirectory;
  }

  // Update the launcher sym link.
  base::FilePath tmp_launcher_name = install_dir->Append("NewCurrent");
  if (!base::DeleteFile(tmp_launcher_name)) {
    VLOG(1) << "Failed to delete existing " << tmp_launcher_name.value();
  }
  if (symlink(kUpdaterVersion, tmp_launcher_name.value().c_str())) {
    return kErrorFailedToLinkLauncher;
  }
  if (rename(tmp_launcher_name.value().c_str(),
             install_dir->Append("Current").value().c_str())) {
    return kErrorFailedToRenameLauncher;
  }
  if (scope == UpdaterScope::kSystem) {
    base::FilePath path =
        bundle_path->Append("Contents").Append("Helpers").Append("launcher");
    struct stat info;
    if (lstat(path.value().c_str(), &info) || info.st_uid ||
        !(S_IFREG & info.st_mode) ||
        lchmod(path.value().c_str(),
               S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH | S_ISUID)) {
      VPLOG(1) << "Launcher lchmod failed. Cross-user on-demand will not work";
      base::debug::DumpWithoutCrashing();
    }
  }

  if (!CreateWakeLaunchdJobPlist(scope)) {
    return kErrorFailedToCreateWakeLaunchdJobPlist;
  }

  if (!InstallKeystone(scope))
    return kErrorFailedToInstallLegacyUpdater;

  return kErrorOk;
}

#pragma mark Uninstall
int UninstallCandidate(UpdaterScope scope) {
  return !DeleteCandidateInstallFolder(scope) ? kErrorFailedToDeleteFolder
                                              : kErrorOk;
}

int Uninstall(UpdaterScope scope) {
  VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
          << " : " << __func__;
  int exit = UninstallCandidate(scope);

  if (!RemoveWakeJobFromLaunchd(scope)) {
    exit = kErrorFailedToRemoveWakeJobFromLaunchd;
  }

  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::WithBaseSyncPrimitives()},
                             base::BindOnce(&UninstallKeystone, scope));

  // Delete Keystone shim plists.
  if (IsSystemInstall(scope)) {
    base::DeleteFile(GetLibraryFolderPath(scope)
                         ->Append("LaunchDaemons")
                         .Append(base::ToLowerASCII(LEGACY_GOOGLE_UPDATE_APPID
                                                    ".daemon.plist")));
  } else {
    base::FilePath launch_agent_dir =
        GetLibraryFolderPath(scope)->Append("LaunchAgents");
    base::DeleteFile(launch_agent_dir.Append(
        base::ToLowerASCII(LEGACY_GOOGLE_UPDATE_APPID ".agent.plist"))) &&
        base::DeleteFile(launch_agent_dir.Append(base::ToLowerASCII(
            LEGACY_GOOGLE_UPDATE_APPID ".xpcservice.plist")));
  }
  // Deleting the install folder is best-effort. Current running processes such
  // as the crash handler process may still write to the updater log file, thus
  // it is not always possible to delete the data folder.
  DeleteFolder(GetInstallDirectory(scope));

  return exit;
}

}  // namespace updater
