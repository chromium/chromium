// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/keystone.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/mac/setup/ks_tickets.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

bool CopyKeystoneBundle(UpdaterScope scope) {
  // The Keystone Bundle is in
  // GoogleUpdater.app/Contents/Helpers/GoogleSoftwareUpdate.bundle.
  base::FilePath keystone_bundle_path =
      base::mac::OuterBundlePath()
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

  // CopyDirectory() does not remove files in destination.
  // Uninstalls the existing Keystone bundle to avoid possible left-over
  // files that breaks bundle signature. A manual delete follows
  // in case uninstall is unsucessful.
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
    if (!base::SetPosixFilePermissions(
            GetLibraryFolderPath(scope)->Append(COMPANY_SHORTNAME_STRING),
            kPermissionsMask) ||
        !base::SetPosixFilePermissions(*GetBaseInstallDirectory(scope),
                                       kPermissionsMask) ||
        !base::SetPosixFilePermissions(*GetVersionedInstallDirectory(scope),
                                       kPermissionsMask) ||
        !base::SetPosixFilePermissions(dest_path, kPermissionsMask)) {
      LOG(ERROR) << "Failed to set permissions to drwxr-xr-x at "
                 << dest_path.value();
      return false;
    }
  }

  DCHECK(!base::DirectoryExists(dest_keystone_bundle_path));
  if (!base::CopyDirectory(keystone_bundle_path, dest_keystone_bundle_path,
                           true)) {
    LOG(ERROR) << "Copying keystone bundle '" << keystone_bundle_path
               << "' to '" << dest_keystone_bundle_path.value() << "' failed.";
    return false;
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
          "com.google.keystone.daemon.plist")) {
    return false;
  }

  base::FilePath launch_agent_dir =
      GetLibraryFolderPath(scope)->Append("LaunchAgents");
  return CreateEmptyFileInDirectory(launch_agent_dir,
                                    "com.google.keystone.agent.plist") &&
         CreateEmptyFileInDirectory(launch_agent_dir,
                                    "com.google.keystone.xpcservice.plist");
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
  if (IsSystemInstall(scope))
    command_line = MakeElevated(command_line);
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

void MigrateKeystoneTickets(
    UpdaterScope scope,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store = [KSTicketStore
        readStoreWithPath:base::SysUTF8ToNSString(
                              GetKeystoneFolderPath(scope)
                                  ->Append(FILE_PATH_LITERAL("TicketStore"))
                                  .Append(
                                      FILE_PATH_LITERAL("Keystone.ticketstore"))
                                  .AsUTF8Unsafe())];

    for (NSString* key in store) {
      KSTicket* ticket = [store objectForKey:key];

      RegistrationRequest registration;
      registration.app_id = base::SysNSStringToUTF8(ticket.productID);
      registration.version =
          base::Version(base::SysNSStringToUTF8([ticket determineVersion]));
      registration.existence_checker_path =
          base::mac::NSStringToFilePath(ticket.existenceChecker.path);
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
          registration.existence_checker_path.empty() ||
          !base::PathExists(registration.existence_checker_path) ||
          !registration.version.IsValid() ||
          base::EqualsCaseInsensitiveASCII(registration.app_id,
                                           "com.google.Keystone")) {
        continue;
      }

      register_callback.Run(registration);
    }
  }
}

}  // namespace updater
