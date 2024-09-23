// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/posix/setup.h"

#import <ServiceManagement/ServiceManagement.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
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
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "components/crash/core/common/crash_key.h"

namespace updater {
namespace {

bool CopyBundle(UpdaterScope scope) {
  std::optional<base::FilePath> base_install_dir = GetInstallDirectory(scope);
  std::optional<base::FilePath> versioned_install_dir =
      GetVersionedInstallDirectory(scope);
  if (!base_install_dir || !versioned_install_dir) {
    LOG(ERROR) << "Failed to get install directory.";
    return false;
  }

  if (base::PathExists(*versioned_install_dir)) {
    if (!DeleteExcept(versioned_install_dir->Append("Crashpad"))) {
      LOG(ERROR) << "Could not remove existing copy of this updater.";
      return false;
    }
  }

  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(*versioned_install_dir, &error)) {
    LOG(ERROR) << "Failed to create '" << versioned_install_dir->value().c_str()
               << "' directory: " << base::File::ErrorToString(error);
    return false;
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

bool BootstrapPlist(UpdaterScope scope, const base::FilePath& path) {
  std::string output;
  int exit_code = 0;
  base::CommandLine launchctl(base::FilePath("/bin/launchctl"));
  launchctl.AppendArg("bootstrap");
  launchctl.AppendArg(GetDomain(scope));
  launchctl.AppendArgPath(path);
  if (!base::GetAppOutputWithExitCode(launchctl, &output, &exit_code) ||
      exit_code != 0) {
    VLOG(1) << "launchctl bootstrap of " << path << " failed: " << exit_code
            << ": " << output;
    return false;
  }
  return true;
}

// Ensure that the LaunchAgents/LaunchDaemons directory contains the wake item
// plist, with the specified contents. If not, the plist will be overwritten and
// the item reloaded. May block.
bool EnsureWakeLaunchItemPresence(UpdaterScope scope, NSDictionary* contents) {
  const std::optional<base::FilePath> path = GetWakeTaskPlistPath(scope);
  if (!path) {
    VLOG(1) << "Failed to find wake plist path.";
    return false;
  }
  const bool previousPlistExists = base::PathExists(*path);
  if (!base::CreateDirectory(path->DirName())) {
    VLOG(1) << "Failed to create " << path->DirName();
    return false;
  }
  @autoreleasepool {
    NSURL* const url = base::apple::FilePathToNSURL(*path);

    // If the file is unchanged, avoid a spammy notification by not touching it.
    if (previousPlistExists &&
        [contents isEqualToDictionary:[NSDictionary
                                          dictionaryWithContentsOfURL:url
                                                                error:nil]]) {
      VLOG(2) << "Skipping unnecessary update to " << path;
      return true;
    }

    // Save a backup of the previous plist.
    base::ScopedTempDir backup_dir;
    if (previousPlistExists &&
        (!backup_dir.CreateUniqueTempDir() ||
         !base::CopyFile(*path, backup_dir.GetPath().Append("backup_plist")))) {
      VLOG(1) << "Failed to back up previous plist.";
      return false;
    }

    // Bootout the old plist.
    {
      std::string output;
      int exit_code = 0;
      base::CommandLine launchctl(base::FilePath("/bin/launchctl"));
      launchctl.AppendArg("bootout");
      launchctl.AppendArg(GetDomain(scope));
      launchctl.AppendArgPath(*path);
      if (!base::GetAppOutputWithExitCode(launchctl, &output, &exit_code)) {
        VLOG(1) << "Failed to launch launchctl.";
      } else if (exit_code != 0) {
        // This is expected in cases where there the service doesn't exist.
        // Unfortunately, in the user case, bootout returns 5 both for does-not-
        // exist errors and other errors.
        VLOG(2) << "launchctl bootout exited: " << exit_code
                << ", stdout: " << output;
      }
    }

    // Update app registration with LaunchServices.
    const std::optional<base::FilePath> install_path =
        GetInstallDirectory(scope);
    if (install_path) {
      OSStatus ls_result = LSRegisterURL(
          base::apple::FilePathToCFURL(
              install_path->Append("Current").Append(base::StrCat(
                  {PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"})))
              .get(),
          true);
      VLOG_IF(1, ls_result != noErr) << "LSRegisterURL failed: " << ls_result;
    } else {
      VLOG(1) << "Failed to retrieve bundle path, skipping LSRegisterURL.";
    }

    // Overwrite the plist.
    NSData* data = [NSPropertyListSerialization
        dataWithPropertyList:contents
                      format:NSPropertyListXMLFormat_v1_0
                     options:0
                       error:nil];
    NSError* error;
    if (![data writeToURL:url options:NSDataWritingAtomic error:&error]) {
      VLOG(1) << "Failed to write " << url << " error " << error.description;
      return false;
    }

    // Bootstrap the new plist.
    if (!BootstrapPlist(scope, *path)) {
      // The plist has already been replaced! If launchctl doesn't like it,
      // this installation is now broken. Try to recover by restoring and
      // bootstrapping the backup.
      if (previousPlistExists &&
          (!base::Move(backup_dir.GetPath().Append("backup_plist"), *path) ||
           !BootstrapPlist(scope, *path))) {
        VLOG(1) << "Failed to restore backup plist.";
      }
      return false;
    }
    return true;
  }
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
  const std::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  if (!install_dir) {
    return kErrorFailedToGetInstallDir;
  }
  if (!PrepareToRunBundle(*install_dir)) {
    VLOG(1) << "PrepareToRunBundle failed. Gatekeeper may prompt.";
  }

  // If there is no Current symlink, create one now.
  base::FilePath current_symlink = install_dir->Append("Current");
  if (!base::PathExists(current_symlink)) {
    if (base::DeleteFile(current_symlink) &&
        symlink(kUpdaterVersion, current_symlink.value().c_str())) {
      return kErrorFailedToLinkCurrent;
    }
  }

  if (!CreateWakeLaunchdJobPlist(scope)) {
    return kErrorFailedToCreateWakeLaunchdJobPlist;
  }

  if (scope == UpdaterScope::kSystem) {
    const std::optional<base::FilePath> bundle_path =
        GetUpdaterAppBundlePath(scope);
    if (bundle_path) {
      base::FilePath path =
          bundle_path->Append("Contents").Append("Helpers").Append("launcher");
      struct stat info;
      if (lstat(path.value().c_str(), &info) || info.st_uid ||
          !(S_IFREG & info.st_mode) ||
          lchmod(path.value().c_str(),
                 S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH | S_ISUID)) {
        VPLOG(1)
            << "Launcher lchmod failed. Cross-user on-demand will not work";
        base::debug::DumpWithoutCrashing();
      }
    }
  }

  return kErrorOk;
}

}  // namespace

int Setup(UpdaterScope scope) {
  int error = DoSetup(scope);
  if (error) {
    CleanAfterInstallFailure(scope);
  }
  return error;
}

int PromoteCandidate(UpdaterScope scope) {
  const std::optional<base::FilePath> updater_executable_path =
      GetUpdaterExecutablePath(scope);
  const std::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  const std::optional<base::FilePath> bundle_path =
      GetUpdaterAppBundlePath(scope);
  if (!updater_executable_path || !install_dir || !bundle_path) {
    return kErrorFailedToGetVersionedInstallDirectory;
  }

  // Update the Current symlink.
  base::FilePath tmp_current_name = install_dir->Append("NewCurrent");
  if (!base::DeleteFile(tmp_current_name)) {
    VLOG(1) << "Failed to delete existing " << tmp_current_name.value();
  }
  if (symlink(kUpdaterVersion, tmp_current_name.value().c_str())) {
    return kErrorFailedToLinkCurrent;
  }
  if (rename(tmp_current_name.value().c_str(),
             install_dir->Append("Current").value().c_str())) {
    return kErrorFailedToRenameCurrent;
  }

  if (!CreateWakeLaunchdJobPlist(scope)) {
    return kErrorFailedToCreateWakeLaunchdJobPlist;
  }

  if (!InstallKeystone(scope)) {
    return kErrorFailedToInstallLegacyUpdater;
  }

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

  // Delete the updater's caches. On Mac, this is different from the
  // install directory.
  DeleteFolder(GetCacheBaseDirectory(scope));
  // Deleting the install folder is best-effort. Current running processes such
  // as the crash handler process may still write to the updater log file, thus
  // it is not always possible to delete the log file. Additionally, the log
  // file is helpful for debugging.
  if (!DeleteExcept(GetLogFilePath(scope))) {
    VLOG(0) << "Failed to delete install directory.";
  }

  return exit;
}

}  // namespace updater
