// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/keystone.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "chrome/updater/mac/mac_util.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"

namespace updater {

namespace {

// Keystone versions look like 1.3.16.180; disregard any Keystone at a version
// greater than 2.0.0.0.
constexpr char kMaxKeystoneVersion[] = "2";

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
  if (!base::PathExists(dest_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dest_path, &error)) {
      LOG(ERROR) << "Failed to create '" << dest_path.value().c_str()
                 << "' directory: " << base::File::ErrorToString(error);
      return false;
    }
  }

  // For system installs, set file permissions to be drwxr-xr-x.
  if (scope == UpdaterScope::kSystem) {
    constexpr int kPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                     base::FILE_PERMISSION_READ_BY_GROUP |
                                     base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                     base::FILE_PERMISSION_READ_BY_OTHERS |
                                     base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
    if (!base::SetPosixFilePermissions(
            GetLibraryFolderPath(scope)->Append(COMPANY_SHORTNAME_STRING),
            kPermissionsMask) ||
        !base::SetPosixFilePermissions(*GetUpdaterFolderPath(scope),
                                       kPermissionsMask) ||
        !base::SetPosixFilePermissions(*GetVersionedUpdaterFolderPath(scope),
                                       kPermissionsMask)) {
      LOG(ERROR) << "Failed to set permissions to drwxr-xr-x at "
                 << dest_path.value().c_str();
      return false;
    }
  }

  if (!base::CopyDirectory(keystone_bundle_path, dest_path, true)) {
    LOG(ERROR) << "Copying keystone bundle '" << keystone_bundle_path
               << "' to '" << dest_path.value().c_str() << "' failed.";
    return false;
  }
  return true;
}

absl::optional<base::FilePath> GetKsadminPath(UpdaterScope scope) {
  const absl::optional<base::FilePath> keystone_folder_path =
      GetKeystoneFolderPath(scope);
  if (!keystone_folder_path || !base::PathExists(*keystone_folder_path))
    return absl::nullopt;
  base::FilePath ksadmin_path =
      keystone_folder_path->Append(FILE_PATH_LITERAL(KEYSTONE_NAME ".bundle"))
          .Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Helpers"))
          .Append(FILE_PATH_LITERAL("ksadmin"));
  if (!base::PathExists(ksadmin_path))
    return absl::nullopt;
  return absl::make_optional(ksadmin_path);
}

// Returns the version of ksadmin.
absl::optional<base::Version> GetKsadminVersion(UpdaterScope scope) {
  absl::optional<base::FilePath> ksadmin_path = GetKsadminPath(scope);
  if (!ksadmin_path)
    return absl::nullopt;
  base::CommandLine ksadmin_version(*ksadmin_path);
  ksadmin_version.AppendSwitch("ksadmin-version");
  int exit_code = -1;
  std::string output;
  base::GetAppOutputWithExitCode(ksadmin_version, &output, &exit_code);
  if (exit_code != 0) {
    VLOG(2) << ksadmin_version.GetCommandLineString()
            << " returned exit code: " << exit_code;
    return absl::nullopt;
  }
  base::Version keystone_version(output);
  if (!keystone_version.IsValid()) {
    VLOG(2) << "Ran " << ksadmin_version.GetCommandLineString()
            << " and cannot parse version number from: " << output;
    return absl::nullopt;
  }
  return absl::make_optional(keystone_version);
}

void MigrateKeystoneTickets(
    UpdaterScope scope,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  absl::optional<base::Version> ksadmin_version = GetKsadminVersion(scope);
  if (!ksadmin_version ||
      *ksadmin_version > base::Version(kMaxKeystoneVersion)) {
    // TODO(crbug.com/1250524): If ksadmin_version > max, we are probably
    // communicating with a ksadmin shim, and we should pass an additional
    // argument to only get legacy Keystone tickets here (not tickets
    // synthesized from Chromium Updater's global prefs). However, that argument
    // does not yet exist, so for now, skip any migration from a shim. Note, it
    // is not safe to assume that migration has already completed just because
    // the ksadmin shim is replaced; it may have been replaced for the other
    // updater scope, or migration may have been interrupted.
    return;
  }

  absl::optional<base::FilePath> ksadmin_path = GetKsadminPath(scope);
  if (!ksadmin_path)
    return;
  base::CommandLine ksadmin_tickets(*ksadmin_path);
  ksadmin_tickets.AppendSwitch("print-tickets");
  switch (scope) {
    case UpdaterScope::kSystem:
      ksadmin_tickets.AppendSwitch("system-store");
      break;
    case UpdaterScope::kUser:
      ksadmin_tickets.AppendSwitch("user-store");
      break;
  }
  int exit_code = -1;
  std::string output;
  base::GetAppOutputWithExitCode(ksadmin_tickets, &output, &exit_code);
  if (exit_code != 0) {
    VLOG(1) << ksadmin_tickets.GetCommandLineString()
            << " returned exit code: " << exit_code;
    return;
  }

  for (RegistrationRequest registration : internal::TicketsToMigrate(output)) {
    // TODO(crbug.com/1250524): Don't register com.google.Keystone.
    register_callback.Run(registration);
  }
}

}  // namespace

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
  if (scope == UpdaterScope::kSystem)
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

bool ConvertKeystone(UpdaterScope scope,
                     base::RepeatingCallback<void(const RegistrationRequest&)>
                         register_callback) {
  // TODO(crbug.com/1250524): This must not run concurrently with Keystone.
  MigrateKeystoneTickets(scope, register_callback);
  // TODO(crbug.com/1250524): Flush prefs, then delete the tickets to mitigate
  // duplicate imports.
  return CopyKeystoneBundle(scope);
}

namespace internal {

std::vector<RegistrationRequest> TicketsToMigrate(
    const std::string& ksadmin_tickets) {
  std::vector<RegistrationRequest> result;

  if (ksadmin_tickets.find("No tickets") != std::string::npos) {
    VLOG(1) << "No tickets";
    return result;
  }

  constexpr char kXCPath[] = "path=";

  for (const std::string& ticket :
       SplitStringUsingSubstr(ksadmin_tickets, "\n>", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    VLOG(1) << "ticket:\n" << ticket;
    if (ticket.empty() || ticket[0] != '<') {
      LOG(ERROR) << "Ticket has unexpected format: " << ticket;
      break;
    }
    base::StringPairs key_value_pairs;
    base::SplitStringIntoKeyValuePairs(ticket, '=', '\n', &key_value_pairs);

    RegistrationRequest registration;
    for (const auto& pair : key_value_pairs) {
      const std::string& key = pair.first;
      const std::string& value = pair.second;
      if (key.empty() || value.empty())
        continue;
      VLOG(1) << "kvp: " << key << "=" << value;
      if (key == "productID") {
        registration.app_id = value;
      } else if (key == "version") {
        registration.version = base::Version(value);
      } else if (key == "xc") {
        if (value.find(kXCPath) == std::string::npos ||
            value.find(">") == std::string::npos) {
          LOG(ERROR) << "Existence checker path not found in: " << value;
          break;
        }
        int start_pos = value.find(kXCPath) + strlen(kXCPath);
        std::string path = value.substr(start_pos, value.find(">") - start_pos);
        VLOG(0) << "path: " << path;
        registration.existence_checker_path = base::FilePath(path);
      } else if (key == "brandPath") {
        // TODO(crbug.com/1250524): Look up the file and read the brand code.
        registration.brand_code = "";
      } else if (key == "tag") {
        registration.tag = value;
      }
      // TODO(crbug.com/1250524): Handle tagPath/tagKey.
    }
    if (!registration.app_id.empty() &&
        !registration.existence_checker_path.empty() &&
        registration.version.IsValid())
      result.push_back(registration);
  }
  return result;
}

}  // namespace internal

}  // namespace updater
