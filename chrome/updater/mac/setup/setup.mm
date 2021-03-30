// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/setup.h"

#import <ServiceManagement/ServiceManagement.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#import "chrome/updater/mac/mac_util.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"

namespace updater {

namespace {

constexpr char kLoggingModuleSwitchValue[] =
    "*/updater/*=2,*/update_client/*=2";

#pragma mark Helpers
const base::FilePath GetUpdaterAppName() {
  return base::FilePath(PRODUCT_FULLNAME_STRING ".app");
}

const base::FilePath GetUpdaterAppExecutablePath() {
  return base::FilePath("Contents/MacOS").AppendASCII(PRODUCT_FULLNAME_STRING);
}

const base::FilePath GetUpdaterExecutablePath(
    const base::FilePath& updater_folder_path) {
  return updater_folder_path.Append(GetUpdaterAppName())
      .Append(GetUpdaterAppExecutablePath());
}

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
bool CopyBundle(const base::FilePath& dest_path) {
  if (!base::PathExists(dest_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dest_path, &error)) {
      LOG(ERROR) << "Failed to create '" << dest_path.value().c_str()
                 << "' directory: " << base::File::ErrorToString(error);
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
  if (scope == UpdaterScope::kSystem)
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetUpdateServiceLaunchdLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_MACHSERVICES : @{GetUpdateServiceMachName() : @YES},
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : NSStringSessionType(scope)
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
    MakeProgramArgument(kWakeSwitch), MakeProgramArgument(kEnableLoggingSwitch)
  ]];
  if (scope == UpdaterScope::kSystem)
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetWakeLaunchdLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_STARTINTERVAL : @3600,
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : NSStringSessionType(scope)
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateUpdateServiceInternalLaunchdPlist(
    UpdaterScope scope,
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSMutableArray<NSString*>* program_arguments =
      [NSMutableArray<NSString*> array];
  [program_arguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(updater_path.value()),
    MakeProgramArgument(kServerSwitch),
    MakeProgramArgumentWithValue(kServerServiceSwitch,
                                 kServerUpdateServiceInternalSwitchValue),
    MakeProgramArgument(kEnableLoggingSwitch),
    MakeProgramArgumentWithValue(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue)
  ]];
  if (scope == UpdaterScope::kSystem)
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetUpdateServiceInternalLaunchdLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_MACHSERVICES : @{GetUpdateServiceInternalMachName() : @YES},
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : NSStringSessionType(scope)
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
      CopyUpdateServiceLaunchdName(), plist);
}

bool CreateWakeLaunchdJobPlist(UpdaterScope scope,
                               const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateWakeLaunchdPlist(scope, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(LaunchdDomain(scope),
                                                  ServiceLaunchdType(scope),
                                                  CopyWakeLaunchdName(), plist);
}

bool CreateUpdateServiceInternalLaunchdJobPlist(
    UpdaterScope scope,
    const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateUpdateServiceInternalLaunchdPlist(scope, updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(scope), ServiceLaunchdType(scope),
      CopyUpdateServiceInternalLaunchdName(), plist);
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
      LaunchdDomain(scope), ServiceLaunchdType(scope), CopyWakeLaunchdName(),
      CFSessionType(scope));
}

bool StartUpdateServiceInternalVersionedLaunchdJob(UpdaterScope scope) {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(scope), ServiceLaunchdType(scope),
      CopyUpdateServiceInternalLaunchdName(), CFSessionType(scope));
}

bool StartLaunchdServiceJob(UpdaterScope scope) {
  return StartUpdateServiceVersionedLaunchdJob(scope,
                                               CopyUpdateServiceLaunchdName());
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
                                           CopyUpdateServiceLaunchdName());
}

bool RemoveUpdateWakeJobFromLaunchd(UpdaterScope scope) {
  return RemoveServiceJobFromLaunchd(scope, CopyWakeLaunchdName());
}

bool RemoveUpdateServiceInternalJobFromLaunchd(UpdaterScope scope) {
  return RemoveServiceJobFromLaunchd(scope,
                                     CopyUpdateServiceInternalLaunchdName());
}

bool DeleteFolder(const base::Optional<base::FilePath>& installed_path) {
  if (!installed_path)
    return false;
  if (!base::DeletePathRecursively(*installed_path)) {
    LOG(ERROR) << "Deleting " << installed_path << " failed";
    return false;
  }
  return true;
}

bool DeleteInstallFolder(UpdaterScope scope) {
  return DeleteFolder(GetUpdaterFolderPath(scope));
}

bool DeleteCandidateInstallFolder(UpdaterScope scope) {
  return DeleteFolder(GetVersionedUpdaterFolderPath(scope));
}

bool DeleteDataFolder() {
  return DeleteFolder(GetBaseDirectory());
}

}  // namespace

int Setup(UpdaterScope scope) {
  const base::Optional<base::FilePath> dest_path =
      GetVersionedUpdaterFolderPath(scope);

  if (!dest_path)
    return setup_exit_codes::kFailedToGetVersionedUpdaterFolderPath;
  if (!CopyBundle(*dest_path))
    return setup_exit_codes::kFailedToCopyBundle;

  const base::FilePath updater_executable_path =
      dest_path->Append(GetUpdaterAppName())
          .Append(GetUpdaterAppExecutablePath());

  if (!CreateWakeLaunchdJobPlist(scope, updater_executable_path))
    return setup_exit_codes::kFailedToCreateWakeLaunchdJobPlist;

  if (!CreateUpdateServiceInternalLaunchdJobPlist(scope,
                                                  updater_executable_path))
    return setup_exit_codes::
        kFailedToCreateUpdateServiceInternalLaunchdJobPlist;

  if (!StartUpdateServiceInternalVersionedLaunchdJob(scope))
    return setup_exit_codes::kFailedToStartLaunchdUpdateServiceInternalJob;

  if (!StartUpdateWakeVersionedLaunchdJob(scope))
    return setup_exit_codes::kFailedToStartLaunchdWakeJob;

  return setup_exit_codes::kSuccess;
}

int PromoteCandidate(UpdaterScope scope) {
  const base::Optional<base::FilePath> dest_path =
      GetVersionedUpdaterFolderPath(scope);
  if (!dest_path)
    return setup_exit_codes::kFailedToGetVersionedUpdaterFolderPath;
  const base::FilePath updater_executable_path =
      dest_path->Append(GetUpdaterAppName())
          .Append(GetUpdaterAppExecutablePath());

  if (!CreateUpdateServiceLaunchdJobPlist(scope, updater_executable_path))
    return setup_exit_codes::kFailedToCreateUpdateServiceLaunchdJobPlist;

  if (!StartLaunchdServiceJob(scope))
    return setup_exit_codes::kFailedToStartLaunchdActiveServiceJob;

  return setup_exit_codes::kSuccess;
}

#pragma mark Uninstall
int UninstallCandidate(UpdaterScope scope) {
  if (!DeleteCandidateInstallFolder(scope))
    return setup_exit_codes::kFailedToDeleteFolder;

  if (!RemoveUpdateWakeJobFromLaunchd(scope))
    return setup_exit_codes::kFailedToRemoveWakeJobFromLaunchd;

  // Removing the Update Internal job has to be the last step because launchd is
  // likely to terminate the current process. Clients should expect the
  // connection to invalidate (possibly with an interruption beforehand) as a
  // result of service uninstallation.
  if (!RemoveUpdateServiceInternalJobFromLaunchd(scope))
    return setup_exit_codes::kFailedToRemoveUpdateServiceInternalJobFromLaunchd;

  return setup_exit_codes::kSuccess;
}

void UninstallOtherVersions(UpdaterScope scope) {
  const base::Optional<base::FilePath> path =
      GetVersionedUpdaterFolderPath(scope);
  if (!path) {
    LOG(ERROR) << "Failed to get updater folder path.";
    return;
  }
  base::FileEnumerator file_enumerator(*path, true,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_folder_path = file_enumerator.Next();
       !version_folder_path.empty() &&
       version_folder_path != GetVersionedUpdaterFolderPath(scope);
       version_folder_path = file_enumerator.Next()) {
    const base::FilePath version_executable_path =
        GetUpdaterExecutablePath(version_folder_path);

    if (base::PathExists(version_executable_path)) {
      base::CommandLine command_line(version_executable_path);
      command_line.AppendSwitch(kUninstallSelfSwitch);
      if (scope == UpdaterScope::kSystem)
        command_line.AppendSwitch(kSystemSwitch);
      command_line.AppendSwitch("--enable-logging");
      command_line.AppendSwitchASCII("--vmodule", "*/chrome/updater/*=2");

      int exit_code = -1;
      std::string output;
      base::GetAppOutputWithExitCode(command_line, &output, &exit_code);
    } else {
      VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
              << " : Path doesn't exist: " << version_executable_path;
    }
  }
}

int Uninstall(UpdaterScope scope) {
  VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
          << " : " << __func__;
  const int exit = UninstallCandidate(scope);
  if (exit != setup_exit_codes::kSuccess)
    return exit;

  if (!RemoveUpdateServiceJobFromLaunchd(scope))
    return setup_exit_codes::kFailedToRemoveActiveUpdateServiceJobFromLaunchd;

  UninstallOtherVersions(scope);

  if (!DeleteDataFolder())
    return setup_exit_codes::kFailedToDeleteDataFolder;

  if (!DeleteInstallFolder(scope))
    return setup_exit_codes::kFailedToDeleteFolder;

  return setup_exit_codes::kSuccess;
}

}  // namespace updater
