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
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"

namespace updater {

namespace {

constexpr char kLoggingModuleSwitchValue[] = "*/updater/*=2";

#pragma mark Helpers
const base::FilePath GetUpdateFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

const base::FilePath GetUpdaterAppName() {
  return base::FilePath(PRODUCT_FULLNAME_STRING ".app");
}

const base::FilePath GetUpdaterAppExecutablePath() {
  return base::FilePath("Contents/MacOS").AppendASCII(PRODUCT_FULLNAME_STRING);
}

bool IsSystemInstall() {
  return geteuid() == 0;
}

const base::FilePath GetLibraryFolderPath() {
  // For user installations: the "~/Library" for the logged in user.
  // For system installations: "/Library".
  if (IsSystemInstall()) {
    base::FilePath local_library_path;
    if (!base::mac::GetLocalDirectory(NSLibraryDirectory,
                                      &local_library_path)) {
      VLOG(1) << "Could not get local library path";
    }
    return local_library_path;
  }
  return base::mac::GetUserLibraryPath();
}

const base::FilePath GetUpdaterFolderPath() {
  // For user installations:
  // ~/Library/COMPANY_SHORTNAME_STRING/PRODUCT_FULLNAME_STRING.
  // e.g. ~/Library/Google/GoogleUpdater
  // For system installations:
  // /Library/COMPANY_SHORTNAME_STRING/PRODUCT_FULLNAME_STRING.
  // e.g. /Library/Google/GoogleUpdater
  return GetLibraryFolderPath().Append(GetUpdateFolderName());
}

const base::FilePath GetVersionedUpdaterFolderPath() {
  return GetUpdaterFolderPath().AppendASCII(UPDATER_VERSION_STRING);
}

const base::FilePath GetUpdaterExecutablePath(
    const base::FilePath& updater_folder_path) {
  return updater_folder_path.Append(GetUpdaterAppName())
      .Append(GetUpdaterAppExecutablePath());
}

Launchd::Domain LaunchdDomain() {
  return IsSystemInstall() ? Launchd::Domain::Local : Launchd::Domain::User;
}

Launchd::Type ServiceLaunchdType() {
  return IsSystemInstall() ? Launchd::Type::Daemon : Launchd::Type::Agent;
}

Launchd::Type ClientLaunchdType() {
  return Launchd::Type::Agent;
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
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetServiceLaunchdLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[
      base::SysUTF8ToNSString(updater_path.value()),
      MakeProgramArgument(kServerSwitch),
      MakeProgramArgumentWithValue(kServerServiceSwitch,
                                   kServerUpdateServiceSwitchValue),
      MakeProgramArgumentWithValue(kLoggingModuleSwitch,
                                   kLoggingModuleSwitchValue),
    ],
    @LAUNCH_JOBKEY_MACHSERVICES : @{GetServiceMachName() : @YES},
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @NO,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateWakeLaunchdPlist(
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSMutableArray<NSString*>* program_arguments =
      [NSMutableArray<NSString*> array];
  [program_arguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(updater_path.value()),
    MakeProgramArgument(kWakeSwitch)
  ]];
  if (IsSystemInstall())
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetWakeLaunchdLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_STARTINTERVAL : @3600,
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @NO,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

base::ScopedCFTypeRef<CFDictionaryRef> CreateControlLaunchdPlist(
    const base::FilePath& updater_path) {
  // See the man page for launchd.plist.
  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : GetControlLaunchdLabel(),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[
      base::SysUTF8ToNSString(updater_path.value()),
      MakeProgramArgument(kServerSwitch),
      MakeProgramArgumentWithValue(kServerServiceSwitch,
                                   kServerControlServiceSwitchValue),
      MakeProgramArgumentWithValue(kLoggingModuleSwitch,
                                   kLoggingModuleSwitchValue),
    ],
    @LAUNCH_JOBKEY_MACHSERVICES : @{GetVersionedServiceMachName() : @YES},
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @NO,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
  };

  return base::ScopedCFTypeRef<CFDictionaryRef>(
      base::mac::CFCast<CFDictionaryRef>(launchd_plist),
      base::scoped_policy::RETAIN);
}

bool CreateUpdateServiceLaunchdJobPlist(const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateServiceLaunchdPlist(updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), ServiceLaunchdType(), CopyServiceLaunchdName(), plist);
}

bool CreateWakeLaunchdJobPlist(const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateWakeLaunchdPlist(updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), ServiceLaunchdType(), CopyWakeLaunchdName(), plist);
}

bool CreateControlLaunchdJobPlist(const base::FilePath& updater_path) {
  // We're creating directories and writing a file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateControlLaunchdPlist(updater_path));
  return Launchd::GetInstance()->WritePlistToFile(
      LaunchdDomain(), ServiceLaunchdType(), CopyControlLaunchdName(), plist);
}

bool StartUpdateServiceVersionedLaunchdJob(
    const base::ScopedCFTypeRef<CFStringRef> name) {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), ServiceLaunchdType(), name, CFSTR("Aqua"));
}

bool StartUpdateWakeVersionedLaunchdJob() {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), ServiceLaunchdType(), CopyWakeLaunchdName(),
      CFSTR("Aqua"));
}

bool StartUpdateControlVersionedLaunchdJob() {
  return Launchd::GetInstance()->RestartJob(
      LaunchdDomain(), ServiceLaunchdType(), CopyControlLaunchdName(),
      CFSTR("Aqua"));
}

bool StartLaunchdServiceJob() {
  return StartUpdateServiceVersionedLaunchdJob(CopyServiceLaunchdName());
}

bool RemoveJobFromLaunchd(Launchd::Domain domain,
                          Launchd::Type type,
                          base::ScopedCFTypeRef<CFStringRef> name) {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // If the job doesn't exist return true.
  if (!Launchd::GetInstance()->PlistExists(domain, type, name))
    return true;

  if (!Launchd::GetInstance()->DeletePlist(domain, type, name))
    return false;

  return Launchd::GetInstance()->RemoveJob(base::SysCFStringRefToUTF8(name));
}

bool RemoveClientJobFromLaunchd(base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveJobFromLaunchd(LaunchdDomain(), ClientLaunchdType(), name);
}

bool RemoveServiceJobFromLaunchd(base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveJobFromLaunchd(LaunchdDomain(), ServiceLaunchdType(), name);
}

bool RemoveUpdateServiceJobFromLaunchd(
    base::ScopedCFTypeRef<CFStringRef> name) {
  return RemoveServiceJobFromLaunchd(name);
}

bool RemoveUpdateServiceJobFromLaunchd() {
  return RemoveUpdateServiceJobFromLaunchd(CopyServiceLaunchdName());
}

bool RemoveUpdateWakeJobFromLaunchd() {
  return RemoveClientJobFromLaunchd(CopyWakeLaunchdName());
}

bool RemoveUpdateControlJobFromLaunchd() {
  return RemoveServiceJobFromLaunchd(CopyControlLaunchdName());
}

bool DeleteFolder(const base::FilePath& installed_path) {
  if (!base::DeletePathRecursively(installed_path)) {
    LOG(ERROR) << "Deleting " << installed_path << " failed";
    return false;
  }
  return true;
}

bool DeleteInstallFolder() {
  return DeleteFolder(GetUpdaterFolderPath());
}

bool DeleteCandidateInstallFolder() {
  return DeleteFolder(GetVersionedUpdaterFolderPath());
}

bool DeleteDataFolder() {
  base::FilePath data_path;
  if (!GetBaseDirectory(&data_path))
    return false;
  return DeleteFolder(data_path);
}

}  // namespace

int Setup() {
  const base::FilePath dest_path = GetVersionedUpdaterFolderPath();

  if (!CopyBundle(dest_path))
    return setup_exit_codes::kFailedToCopyBundle;

  const base::FilePath updater_executable_path =
      dest_path.Append(GetUpdaterAppName())
          .Append(GetUpdaterAppExecutablePath());

  if (!CreateWakeLaunchdJobPlist(updater_executable_path))
    return setup_exit_codes::kFailedToCreateWakeLaunchdJobPlist;

  if (!CreateControlLaunchdJobPlist(updater_executable_path))
    return setup_exit_codes::kFailedToCreateControlLaunchdJobPlist;

  if (!StartUpdateControlVersionedLaunchdJob())
    return setup_exit_codes::kFailedToStartLaunchdControlJob;

  if (!StartUpdateWakeVersionedLaunchdJob())
    return setup_exit_codes::kFailedToStartLaunchdWakeJob;

  return setup_exit_codes::kSuccess;
}

int PromoteCandidate() {
  const base::FilePath dest_path = GetVersionedUpdaterFolderPath();
  const base::FilePath updater_executable_path =
      dest_path.Append(GetUpdaterAppName())
          .Append(GetUpdaterAppExecutablePath());

  if (!CreateUpdateServiceLaunchdJobPlist(updater_executable_path))
    return setup_exit_codes::kFailedToCreateUpdateServiceLaunchdJobPlist;

  if (!StartLaunchdServiceJob())
    return setup_exit_codes::kFailedToStartLaunchdActiveServiceJob;

  return setup_exit_codes::kSuccess;
}

#pragma mark Uninstall
int UninstallCandidate() {
  if (!RemoveUpdateWakeJobFromLaunchd())
    return setup_exit_codes::kFailedToRemoveWakeJobFromLaunchd;

  if (!RemoveUpdateControlJobFromLaunchd())
    return setup_exit_codes::kFailedToRemoveControlJobFromLaunchd;

  if (!DeleteCandidateInstallFolder())
    return setup_exit_codes::kFailedToDeleteFolder;

  return setup_exit_codes::kSuccess;
}

void UninstallOtherVersions() {
  base::FileEnumerator file_enumerator(GetUpdaterFolderPath(), true,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_folder_path = file_enumerator.Next();
       !version_folder_path.empty() &&
       version_folder_path != GetVersionedUpdaterFolderPath();
       version_folder_path = file_enumerator.Next()) {
    const base::FilePath version_executable_path =
        GetUpdaterExecutablePath(version_folder_path);

    if (base::PathExists(version_executable_path)) {
      base::CommandLine command_line(version_executable_path);
      command_line.AppendSwitchASCII(kUninstallSwitch, "self");
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

int Uninstall(bool is_machine) {
  ALLOW_UNUSED_LOCAL(is_machine);
  VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
          << " : " << __func__;
  const int exit = UninstallCandidate();
  if (exit != setup_exit_codes::kSuccess)
    return exit;

  if (!RemoveUpdateServiceJobFromLaunchd())
    return setup_exit_codes::kFailedToRemoveActiveUpdateServiceJobFromLaunchd;

  UninstallOtherVersions();

  if (!DeleteDataFolder())
    return setup_exit_codes::kFailedToDeleteDataFolder;

  if (!DeleteInstallFolder())
    return setup_exit_codes::kFailedToDeleteFolder;

  return setup_exit_codes::kSuccess;
}

}  // namespace updater
