// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/posix/setup.h"

#import <ServiceManagement/ServiceManagement.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/mac/setup/keystone.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/launchd_util.h"
#import "chrome/updater/util/mac_util.h"
#import "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

#pragma mark Helpers
Launchd::Domain LaunchdDomain(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return Launchd::Domain::Local;
    case UpdaterScope::kUser:
      return Launchd::Domain::User;
  }
}

Launchd::Type ServiceLaunchdType(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return Launchd::Type::Daemon;
    case UpdaterScope::kUser:
      return Launchd::Type::Agent;
  }
}

CFStringRef CFSessionType(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return CFSTR("System");
    case UpdaterScope::kUser:
      return CFSTR("Aqua");
  }
}

NSString* NSStringSessionType(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return @"System";
    case UpdaterScope::kUser:
      return @"Aqua";
  }
}

#pragma mark Setup
bool CopyBundle(const base::FilePath& dest_path, UpdaterScope scope) {
  if (!base::PathExists(dest_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dest_path, &error)) {
      LOG(ERROR) << "Failed to create '" << dest_path.value().c_str()
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
    if (!base::SetPosixFilePermissions(
            GetLibraryFolderPath(scope)->Append(COMPANY_SHORTNAME_STRING),
            kPermissionsMask) ||
        !base::SetPosixFilePermissions(*GetBaseInstallDirectory(scope),
                                       kPermissionsMask) ||
        !base::SetPosixFilePermissions(*GetVersionedInstallDirectory(scope),
                                       kPermissionsMask)) {
      LOG(ERROR) << "Failed to set permissions to drwxr-xr-x at "
                 << dest_path.value().c_str();
      return false;
    }
  }

  if (!base::CopyDirectory(base::mac::OuterBundlePath(), dest_path, true)) {
    LOG(ERROR) << "Copying app to '" << dest_path.value().c_str() << "' failed";
    return false;
  }

  return true;
}

NSString* MakeProgramArgument(const char* argument) {
  return base::SysUTF8ToNSString(base::StrCat({"--", argument}));
}

NSString* MakeProgramArgumentWithValue(const char* argument,
                                       const char* value) {
  return base::SysUTF8ToNSString(base::StrCat({"--", argument, "=", value}));
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateServiceLaunchdPlist(
    UpdaterScope scope,
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSMutableArray<NSString*>* program_arguments =
      [NSMutableArray<NSString*> array];
  [program_arguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(updater_path.value()),
    MakeProgramArgument(kServerSwitch),
    MakeProgramArgumentWithValue(kServerServiceSwitch,
                                 kServerUpdateServiceSwitchValue),
    MakeProgramArgument(kEnableLoggingSwitch),
    MakeProgramArgumentWithValue(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue)

  ]];
  if (IsSystemInstall(scope))
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetUpdateServiceLaunchdLabel(scope),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_MACHSERVICES : @{GetUpdateServiceMachName(scope) : @YES},
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : NSStringSessionType(scope),
    @"AssociatedBundleIdentifiers" : @MAC_BUNDLE_IDENTIFIER_STRING
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateWakeLaunchdPlist(
    UpdaterScope scope,
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSMutableArray<NSString*>* program_arguments =
      [NSMutableArray<NSString*> array];
  [program_arguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(updater_path.value()),
    MakeProgramArgument(kWakeAllSwitch),
    MakeProgramArgument(kEnableLoggingSwitch),
    MakeProgramArgumentWithValue(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue)
  ]];
  if (IsSystemInstall(scope))
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetWakeLaunchdLabel(scope),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_STARTINTERVAL : @3600,
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : NSStringSessionType(scope),
    @"AssociatedBundleIdentifiers" : @MAC_BUNDLE_IDENTIFIER_STRING
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

bool CreateUpdateServiceLaunchdJobPlist(UpdaterScope scope,
                                        const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateServiceLaunchdPlist(scope, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(scope), ServiceLaunchdType(scope),
      CopyUpdateServiceLaunchdName(scope), plist);
}

bool CreateWakeLaunchdJobPlist(UpdaterScope scope,
                               const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateWakeLaunchdPlist(scope, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(scope), ServiceLaunchdType(scope),
      CopyWakeLaunchdName(scope), plist);
}

bool StartUpdateServiceVersionedLaunchdJob(
    UpdaterScope scope,
    const base::ScopedCFTypeRef<CFStringRef> name) {
  return Launchd::GetInstance()->RestartJob(LaunchdDomain(scope),
                                            ServiceLaunchdType(scope), name,
                                            CFSessionType(scope));
}

bool StartUpdateWakeVersionedLaunchdJob(UpdaterScope scope) {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(scope), ServiceLaunchdType(scope),
      CopyWakeLaunchdName(scope), CFSessionType(scope));
}

bool StartLaunchdServiceJob(UpdaterScope scope) {
  return StartUpdateServiceVersionedLaunchdJob(
      scope, CopyUpdateServiceLaunchdName(scope));
}

bool RemoveServiceJobFromLaunchd(UpdaterScope scope,
                                 base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveJobFromLaunchd(scope, LaunchdDomain(scope),
                              ServiceLaunchdType(scope), name);
}

bool RemoveUpdateServiceJobFromLaunchd(
    UpdaterScope scope,
    base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveServiceJobFromLaunchd(scope, name);
}

bool RemoveUpdateServiceJobFromLaunchd(UpdaterScope scope) {
  return RemoveUpdateServiceJobFromLaunchd(scope,
                                           CopyUpdateServiceLaunchdName(scope));
}

bool RemoveUpdateWakeJobFromLaunchd(UpdaterScope scope) {
  return RemoveServiceJobFromLaunchd(scope, CopyWakeLaunchdName(scope));
}

bool DeleteInstallFolder(UpdaterScope scope) {
  return DeleteFolder(GetBaseInstallDirectory(scope));
}

bool DeleteDataFolder(UpdaterScope scope) {
  return DeleteFolder(GetBaseDataDirectory(scope));
}

void CleanAfterInstallFailure(UpdaterScope scope) {
  // If install fails at any point, attempt to clean the install.
  DeleteCandidateInstallFolder(scope);
}

bool RemoveQuarantineAttributes(const base::FilePath& updater_bundle_path,
                                const base::FilePath& updater_executable_path) {
  if (!base::PathExists(updater_bundle_path)) {
    VPLOG(1) << "Updater bundle path not found: "
             << updater_bundle_path.value();
    return false;
  }

  if (!base::mac::RemoveQuarantineAttribute(updater_bundle_path)) {
    VPLOG(1) << "Could not remove com.apple.quarantine for the bundle.";
    return false;
  }

  if (!base::mac::RemoveQuarantineAttribute(updater_executable_path)) {
    VPLOG(1) << "Could not remove com.apple.quarantine for the "
                "executable.";
    return false;
  }

  return true;
}

int DoSetup(UpdaterScope scope) {
  const absl::optional<base::FilePath> dest_path =
      GetVersionedInstallDirectory(scope);

  if (!dest_path)
    return kErrorFailedToGetVersionedInstallDirectory;
  if (!CopyBundle(*dest_path, scope))
    return kErrorFailedToCopyBundle;

  const base::FilePath updater_executable_path =
      dest_path->Append(GetExecutableRelativePath());

  // Quarantine attribute needs to be removed here as the copied bundle might be
  // given com.apple.quarantine attribute, and the server is attempted to be
  // launched below, Gatekeeper could prompt the user.
  const absl::optional<base::FilePath> bundle_path =
      GetUpdaterAppBundlePath(scope);
  if (!bundle_path)
    return kErrorFailedToGetAppBundlePath;
  if (!RemoveQuarantineAttributes(*bundle_path, updater_executable_path)) {
    VLOG(1) << "Couldn't remove quarantine bits for updater. This will likely "
               "cause Gatekeeper to show a prompt to the user.";
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
  const absl::optional<base::FilePath> install_dir =
      GetBaseInstallDirectory(scope);
  const absl::optional<base::FilePath> bundle_path =
      GetUpdaterAppBundlePath(scope);
  if (!updater_executable_path || !bundle_path || !install_dir) {
    return kErrorFailedToGetVersionedInstallDirectory;
  }

  // Update the launcher hard link.
  base::FilePath tmp_launcher_name = install_dir->Append("launcher_new");
  if (link(bundle_path->Append("Contents")
               .Append("Helpers")
               .Append("launcher")
               .value()
               .c_str(),
           tmp_launcher_name.value().c_str())) {
    return kErrorFailedToLinkLauncher;
  }
  if (rename(tmp_launcher_name.value().c_str(),
             install_dir->Append("launcher").value().c_str())) {
    return kErrorFailedToRenameLauncher;
  }
  // TODO(crbug.com/1339108): If kSystem, mark setuid on the launcher.

  if (!CreateWakeLaunchdJobPlist(scope, *updater_executable_path)) {
    return kErrorFailedToCreateWakeLaunchdJobPlist;
  }

  if (!CreateUpdateServiceLaunchdJobPlist(scope, *updater_executable_path)) {
    return kErrorFailedToCreateUpdateServiceLaunchdJobPlist;
  }

  if (!StartUpdateWakeVersionedLaunchdJob(scope))
    return kErrorFailedToStartLaunchdWakeJob;

  if (!StartLaunchdServiceJob(scope))
    return kErrorFailedToStartLaunchdActiveServiceJob;

  if (!InstallKeystone(scope))
    return kErrorFailedToInstallLegacyUpdater;

  // Wait for launchd to finish the load operation for the update service.
  base::PlatformThread::Sleep(base::Seconds(2));

  return kErrorOk;
}

#pragma mark Uninstall
int UninstallCandidate(UpdaterScope scope) {
  return !DeleteCandidateInstallFolder(scope) ||
                 !DeleteFolder(GetVersionedDataDirectory(scope))
             ? kErrorFailedToDeleteFolder
             : kErrorOk;
}

int Uninstall(UpdaterScope scope) {
  VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
          << " : " << __func__;
  int exit = UninstallCandidate(scope);

  if (!RemoveUpdateServiceJobFromLaunchd(scope))
    exit = kErrorFailedToRemoveActiveUpdateServiceJobFromLaunchd;

  if (!RemoveUpdateWakeJobFromLaunchd(scope))
    exit = kErrorFailedToRemoveWakeJobFromLaunchd;

  if (!DeleteInstallFolder(scope))
    exit = kErrorFailedToDeleteFolder;

  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::WithBaseSyncPrimitives()},
                             base::BindOnce(&UninstallKeystone, scope));

  // Deleting the data folder is best-effort. Current running processes such as
  // the crash handler process may still write to the updater log file, thus
  // it is not always possible to delete the data folder.
  DeleteDataFolder(scope);

  return exit;
}

}  // namespace updater
