// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/keystone.h"

#include <string>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/mac/setup/ks_tickets.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Class to read the Keystone apps' client-regulated-counting data.
@interface CountingMetricsStore : NSObject {
  NSDictionary<NSString*, NSDictionary<NSString*, id>*>* __strong _metrics;
}

+ (instancetype)storeAtPath:(const base::FilePath&)path;

- (absl::optional<int>)dateLastActiveForApp:(NSString*)appid;
- (absl::optional<int>)dateLastRollcallForApp:(NSString*)appid;

@end

@implementation CountingMetricsStore

+ (instancetype)storeAtPath:(const base::FilePath&)path {
  return [[CountingMetricsStore alloc]
      initWithURL:[base::mac::FilePathToNSURL(path)
                      URLByAppendingPathComponent:@"CountingMetrics.plist"]];
}

- (instancetype)initWithURL:(NSURL*)url {
  if ((self = [super init])) {
    NSError* error = nil;
    _metrics = [[NSDictionary alloc] initWithContentsOfURL:url error:&error];

    if (error) {
      LOG(WARNING) << "Failed to read client-regulated-counting data.";
      self = nil;
    }
  }
  return self;
}

- (absl::optional<int>)daynumValueOfKey:(NSString*)key forApp:(NSString*)appid {
  id appObject = [_metrics objectForKey:appid.lowercaseString];
  if (![appObject isKindOfClass:[NSDictionary class]]) {
    LOG(WARNING) << "Malformed input client-regulated-counting data.";
    return absl::nullopt;
  }

  id daynumObject = appObject[key];
  if (!daynumObject) {
    return absl::nullopt;
  }

  if (![daynumObject isKindOfClass:[NSNumber class]]) {
    LOG(WARNING) << "daynum is not a number.";
    return absl::nullopt;
  }

  // daynum the number of days since January 1, 2007. The accepted range is
  // between 3000 (maps to Mar 20, 2015) and 50000 (maps to Nov 24, 2143).
  int daynum = [daynumObject intValue];
  if (daynum < 3000 || daynum > 50000) {
    LOG(WARNING) << "Ignored out-of-range daynum: " << daynum;
    return absl::nullopt;
  }

  return daynum;
}

- (absl::optional<int>)dateLastActiveForApp:(NSString*)appid {
  return [self daynumValueOfKey:@"DayOfLastActive" forApp:appid];
}

- (absl::optional<int>)dateLastRollcallForApp:(NSString*)appid {
  return [self daynumValueOfKey:@"DayOfLastRollcall" forApp:appid];
}

@end

namespace updater {

namespace {

bool CopyKeystoneBundle(UpdaterScope scope) {
  // The Keystone Bundle is in
  // GoogleUpdater.app/Contents/Helpers/GoogleSoftwareUpdate.bundle.
  base::FilePath keystone_bundle_path =
      base::apple::OuterBundlePath()
          .Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Helpers"))
          .Append(FILE_PATH_LITERAL(KEYSTONE_NAME ".bundle"));

  if (!base::PathExists(keystone_bundle_path)) {
    LOG(ERROR) << "Path to the Keystone bundle does not exist! "
               << keystone_bundle_path;
    return false;
  }

  const absl::optional<base::FilePath> dest_folder_path =
      GetKeystoneFolderPath(scope);
  if (!dest_folder_path)
    return false;
  const base::FilePath dest_path = *dest_folder_path;

  // CopyDir() does not remove files in destination.
  // Uninstalls the existing Keystone bundle to avoid possible left-over
  // files that breaks bundle signature. A manual delete follows
  // in case uninstall is unsuccessful.
  UninstallKeystone(scope);
  const base::FilePath dest_keystone_bundle_path =
      dest_path.Append(FILE_PATH_LITERAL(KEYSTONE_NAME ".bundle"));
  if (base::PathExists(dest_keystone_bundle_path) &&
      !base::DeletePathRecursively(dest_keystone_bundle_path)) {
    LOG(ERROR) << "Failed to delete existing Keystone bundle path.";
    return false;
  }

  if (!base::PathExists(dest_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dest_path, &error)) {
      LOG(ERROR) << "Failed to create '" << dest_path
                 << "' directory: " << base::File::ErrorToString(error);
      return false;
    }
  }

  // For system installs, set file permissions to be drwxr-xr-x.
  if (IsSystemInstall(scope)) {
    constexpr int kPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                     base::FILE_PERMISSION_READ_BY_GROUP |
                                     base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                     base::FILE_PERMISSION_READ_BY_OTHERS |
                                     base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
    if (!base::SetPosixFilePermissions(dest_path.DirName(), kPermissionsMask) ||
        !base::SetPosixFilePermissions(dest_path, kPermissionsMask)) {
      LOG(ERROR) << "Failed to set permissions to drwxr-xr-x at "
                 << dest_path.value();
      return false;
    }
  }

  if (!CopyDir(keystone_bundle_path, dest_path,
               scope == UpdaterScope::kSystem)) {
    LOG(ERROR) << "Copying keystone bundle '" << keystone_bundle_path
               << "' to '" << dest_keystone_bundle_path.value() << "' failed.";
    return false;
  }

  if (!RemoveQuarantineAttributes(dest_keystone_bundle_path)) {
    VLOG(1) << "Couldn't remove quarantine bits for Keystone.";
  }

  return true;
}

bool CreateEmptyFileInDirectory(const base::FilePath& dir,
                                const std::string& file_name) {
  constexpr int kPermissionsMask = base::FILE_PERMISSION_READ_BY_USER |
                                   base::FILE_PERMISSION_WRITE_BY_USER |
                                   base::FILE_PERMISSION_READ_BY_GROUP |
                                   base::FILE_PERMISSION_READ_BY_OTHERS;

  if (!base::PathExists(dir)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dir, &error) ||
        !base::SetPosixFilePermissions(dir, kPermissionsMask)) {
      LOG(ERROR) << "Failed to create '" << dir.value().c_str()
                 << "': " << base::File::ErrorToString(error);
      return false;
    }
  }

  base::FilePath file_path = dir.AppendASCII(file_name);
  int64_t file_size;
  if (base::GetFileSize(file_path, &file_size) && file_size == 0) {
    VLOG(1) << "Skipping creation of " << file_path << ": file already empty.";
    return true;
  }
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.Close();

  if (!base::PathExists(file_path)) {
    LOG(ERROR) << "Failed to create file: " << file_path.value().c_str();
    return false;
  }
  if (!base::SetPosixFilePermissions(file_path, kPermissionsMask)) {
    LOG(ERROR) << "Failed to set permissions: " << file_path.value().c_str();
    return false;
  }

  return true;
}

bool CreateKeystoneLaunchCtlPlistFiles(UpdaterScope scope) {
  // If not all Keystone launchctl plist files are present, Keystone installer
  // will proceed regardless of the bundle state. The empty launchctl files
  // created here are used to make legacy Keystone installer believe that a
  // healthy newer version updater already exists and thus won't over-install.
  if (IsSystemInstall(scope) &&
      !CreateEmptyFileInDirectory(
          GetLibraryFolderPath(scope)->Append("LaunchDaemons"),
          base::ToLowerASCII(LEGACY_GOOGLE_UPDATE_APPID ".daemon.plist"))) {
    return false;
  }

  base::FilePath launch_agent_dir =
      GetLibraryFolderPath(scope)->Append("LaunchAgents");
  return CreateEmptyFileInDirectory(
             launch_agent_dir,
             base::ToLowerASCII(LEGACY_GOOGLE_UPDATE_APPID ".agent.plist")) &&
         CreateEmptyFileInDirectory(
             launch_agent_dir, base::ToLowerASCII(LEGACY_GOOGLE_UPDATE_APPID
                                                  ".xpcservice.plist"));
}

}  // namespace

bool InstallKeystone(UpdaterScope scope) {
  return CopyKeystoneBundle(scope) && CreateKeystoneLaunchCtlPlistFiles(scope);
}

void UninstallKeystone(UpdaterScope scope) {
  const absl::optional<base::FilePath> keystone_folder_path =
      GetKeystoneFolderPath(scope);
  if (!keystone_folder_path) {
    LOG(ERROR) << "Can't find Keystone path.";
    return;
  }
  if (!base::PathExists(*keystone_folder_path)) {
    LOG(ERROR) << "Keystone path '" << *keystone_folder_path
               << "' doesn't exist.";
    return;
  }

  base::FilePath ksinstall_path =
      keystone_folder_path->Append(FILE_PATH_LITERAL(KEYSTONE_NAME ".bundle"))
          .Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Helpers"))
          .Append(FILE_PATH_LITERAL("ksinstall"));
  base::CommandLine command_line(ksinstall_path);
  command_line.AppendSwitch("uninstall");
  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch ksinstall.";
    return;
  }
  int exit_code = 0;

  if (!process.WaitForExitWithTimeout(base::Seconds(30), &exit_code)) {
    LOG(ERROR) << "Uninstall Keystone didn't finish in the allowed time.";
    return;
  }
  if (exit_code != 0) {
    LOG(ERROR) << "Uninstall Keystone returned exit code: " << exit_code << ".";
  }
}

bool MigrateKeystoneApps(
    const base::FilePath& keystone_path,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store = [KSTicketStore
        readStoreWithPath:base::SysUTF8ToNSString(
                              keystone_path
                                  .Append(FILE_PATH_LITERAL("TicketStore"))
                                  .Append(
                                      FILE_PATH_LITERAL("Keystone.ticketstore"))
                                  .AsUTF8Unsafe())];
    if (!store) {
      return false;
    }

    CountingMetricsStore* metrics_store =
        [CountingMetricsStore storeAtPath:keystone_path];

    for (NSString* key in store) {
      KSTicket* ticket = [store objectForKey:key];

      RegistrationRequest registration;
      registration.app_id = base::SysNSStringToUTF8(ticket.productID);
      const base::Version version(
          base::SysNSStringToUTF8([ticket determineVersion]));
      if (version.IsValid()) {
        registration.version = version;
      } else {
        registration.version = base::Version(kNullVersion);
      }
      if (ticket.existenceChecker) {
        registration.existence_checker_path =
            base::mac::NSStringToFilePath(ticket.existenceChecker.path);
      }
      registration.brand_code =
          base::SysNSStringToUTF8([ticket determineBrand]);
      if ([ticket.brandKey isEqualToString:kCRUTicketBrandKey]) {
        // New updater only supports hard-coded brandKey, only migrate brand
        // path if the key matches.
        registration.brand_path =
            base::mac::NSStringToFilePath(ticket.brandPath);
      }
      registration.ap = base::SysNSStringToUTF8([ticket determineTag]);

      // Skip migration for incomplete ticket or Keystone itself.
      if (registration.app_id.empty() ||
          base::EqualsCaseInsensitiveASCII(registration.app_id,
                                           "com.google.Keystone")) {
        continue;
      }

      registration.dla = [metrics_store dateLastActiveForApp:ticket.productID];
      registration.dlrc =
          [metrics_store dateLastRollcallForApp:ticket.productID];

      registration.cohort = base::SysNSStringToUTF8(ticket.cohort);
      registration.cohort_name = base::SysNSStringToUTF8(ticket.cohortName);
      registration.cohort_hint = base::SysNSStringToUTF8(ticket.cohortHint);

      register_callback.Run(registration);
    }
  }
  return true;
}

}  // namespace updater
