// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/util/mac_util.h"

#import <CoreFoundation/CoreFoundation.h>

#include <optional>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/setup/keystone.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"

namespace updater {
namespace {

constexpr base::FilePath::CharType kZipExePath[] =
    FILE_PATH_LITERAL("/usr/bin/unzip");

constexpr base::FilePath::CharType kGkToolPath[] =
    FILE_PATH_LITERAL("/usr/bin/gktool");

base::FilePath ExecutableFolderPath() {
  return base::FilePath(
             base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"));
}

// Recursively remove quarantine attributes on the path. Emits a log message
// if it fails.
bool RemoveQuarantineAttributes(const base::FilePath& updater_bundle_path) {
  bool success = base::mac::RemoveQuarantineAttribute(updater_bundle_path);
  base::FileEnumerator file_enumerator(
      base::FilePath(updater_bundle_path), true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    success = base::mac::RemoveQuarantineAttribute(name) && success;
  }

  VLOG_IF(0, !success) << "Failed to remove quarantine attributes from "
                       << updater_bundle_path;
  return success;
}

// On supported versions of macOS, scan the specified bundle with Gatekeeper
// so it won't pop up a user-visible "Verifying..." box for the duration of
// the scan when an executable in the bundle is later launched for the first
// time. On unsupported macOS versions, this does nothing and returns 0.
//
// On supported macOS versions, this returns the return code from `gktool`.
// If attempting to launch `gktool` fails, this returns -1.
int PrewarmGatekeeperIfSupported(const base::FilePath& bundle_path) {
  // gktool is only available on macOS 14 and later.
  if (@available(macOS 14, *)) {
    base::FilePath tool_path(kGkToolPath);
    base::CommandLine command(tool_path);
    command.AppendArg("scan");
    command.AppendArg(bundle_path.value());

    std::string output;
    int exit_code = -1;
    if (!base::GetAppOutputWithExitCode(command, &output, &exit_code)) {
      VLOG(0) << "Something went wrong trying to run gktool from "
              << kGkToolPath;
      return -1;
    }

    VLOG_IF(0, exit_code) << "gktool returned " << exit_code;
    VLOG_IF(0, exit_code) << "gktool output: " << output;

    return exit_code;
  }
  return 0;
}

}  // namespace

std::string GetDomain(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return "system";
    case UpdaterScope::kUser:
      return base::StrCat({"gui/", base::NumberToString(geteuid())});
  }
}

std::optional<base::FilePath> GetLibraryFolderPath(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return base::apple::GetUserLibraryPath();
    case UpdaterScope::kSystem: {
      base::FilePath local_library_path;
      if (!base::apple::GetLocalDirectory(NSLibraryDirectory,
                                          &local_library_path)) {
        VLOG(1) << "Could not get local library path";
        return std::nullopt;
      }
      return local_library_path;
    }
  }
}

std::optional<base::FilePath> GetApplicationSupportDirectory(
    UpdaterScope scope) {
  base::FilePath path;
  switch (scope) {
    case UpdaterScope::kUser:
      if (base::apple::GetUserDirectory(NSApplicationSupportDirectory, &path)) {
        return path;
      }
      break;
    case UpdaterScope::kSystem:
      if (base::apple::GetLocalDirectory(NSApplicationSupportDirectory,
                                         &path)) {
        return path;
      }
      break;
  }

  VLOG(1) << "Could not get applications support path";
  return std::nullopt;
}

std::optional<base::FilePath> GetKSAdminPath(UpdaterScope scope) {
  const std::optional<base::FilePath> keystone_folder_path =
      GetKeystoneFolderPath(scope);
  if (!keystone_folder_path) {
    return std::nullopt;
  }
  return std::make_optional(
      keystone_folder_path->Append(FILE_PATH_LITERAL(KEYSTONE_NAME ".bundle"))
          .Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Helpers"))
          .Append(FILE_PATH_LITERAL("ksadmin")));
}

std::string GetWakeLaunchdName(UpdaterScope scope) {
  return IsSystemInstall(scope) ? MAC_BUNDLE_IDENTIFIER_STRING ".wake.system"
                                : MAC_BUNDLE_IDENTIFIER_STRING ".wake";
}

bool RemoveWakeJobFromLaunchd(UpdaterScope scope) {
  const std::optional<base::FilePath> path = GetWakeTaskPlistPath(scope);
  if (!path) {
    return false;
  }

  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::CommandLine command_line(base::FilePath("/bin/launchctl"));
  command_line.AppendArg("bootout");
  command_line.AppendArg(GetDomain(scope));
  command_line.AppendArgPath(*path);
  int exit_code = -1;
  std::string output;
  if (base::GetAppOutputWithExitCode(command_line, &output, &exit_code) &&
      exit_code != 0) {
    VLOG(2) << "launchctl bootout exited " << exit_code << ": " << output;
  }
  return base::DeleteFile(*path);
}

bool UnzipWithExe(const base::FilePath& src_path,
                  const base::FilePath& dest_path) {
  base::FilePath file_path(kZipExePath);
  base::CommandLine command(file_path);
  command.AppendArg(src_path.value());
  command.AppendArg("-d");
  command.AppendArg(dest_path.value());

  std::string output;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command, &output, &exit_code)) {
    VLOG(0) << "Something went wrong while running the unzipping with "
            << kZipExePath;
    return false;
  }

  // Unzip utility having 0 is success and 1 is a warning.
  if (exit_code > 1) {
    VLOG(0) << "Output from unzipping: " << output;
    VLOG(0) << "Exit code: " << exit_code;
  }

  return exit_code <= 1;
}

std::optional<base::FilePath> GetExecutableFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version) {
  std::optional<base::FilePath> path =
      GetVersionedInstallDirectory(scope, version);
  if (!path) {
    return std::nullopt;
  }
  return path->Append(ExecutableFolderPath());
}

std::optional<base::FilePath> GetUpdaterAppBundlePath(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetVersionedInstallDirectory(scope);
  if (!path) {
    return std::nullopt;
  }
  return path->Append(
      base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"}));
}

base::FilePath GetExecutableRelativePath() {
  return ExecutableFolderPath().Append(
      base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix}));
}

std::optional<base::FilePath> GetKeystoneFolderPath(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  if (!path) {
    return std::nullopt;
  }
  return path->Append(FILE_PATH_LITERAL(COMPANY_SHORTNAME_STRING))
      .Append(FILE_PATH_LITERAL(KEYSTONE_NAME));
}

bool ConfirmFilePermissions(const base::FilePath& root_path,
                            int kPermissionsMask) {
  base::FileEnumerator file_enumerator(
      root_path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS);

  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    if (!SetPosixFilePermissions(path, kPermissionsMask)) {
      VLOG(0) << "Couldn't set file permissions for for: " << path.value();
      return false;
    }

    base::File::Info file_info;
    if (!base::GetFileInfo(path, &file_info)) {
      VLOG(0) << "Couldn't get file info for: " << path.value();
      return false;
    }

    // If file path is real directory and not a link, recurse into it.
    if (file_info.is_directory && !base::IsLink(path)) {
      if (!ConfirmFilePermissions(path, kPermissionsMask)) {
        return false;
      }
    }
  }

  return true;
}

std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  return path ? std::optional<base::FilePath>(
                    path->Append("Application Support")
                        .Append(COMPANY_SHORTNAME_STRING)
                        .Append(PRODUCT_FULLNAME_STRING))
              : std::nullopt;
}

std::optional<base::FilePath> GetCacheBaseDirectory(UpdaterScope scope) {
  base::FilePath caches_path;
  if (!base::apple::GetLocalDirectory(NSCachesDirectory, &caches_path)) {
    VLOG(1) << "Could not get Caches path";
    return std::nullopt;
  }
  return std::optional<base::FilePath>(
      caches_path.AppendASCII(MAC_BUNDLE_IDENTIFIER_STRING));
}

std::optional<base::FilePath> GetUpdateServiceLauncherPath(UpdaterScope scope) {
  std::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  return install_dir
             ? std::optional<base::FilePath>(
                   install_dir->Append("Current")
                       .Append(base::StrCat({PRODUCT_FULLNAME_STRING,
                                             kExecutableSuffix, ".app"}))
                       .Append("Contents")
                       .Append("Helpers")
                       .Append("launcher"))
             : std::nullopt;
}

bool PrepareToRunBundle(const base::FilePath& bundle_path) {
  // Do not return early. Cleaning up attributes and prewarming Gatekeeper
  // avoids popups visible to the user, but we must continue to try to update
  // even if these fail, so we should do as much of the prep as we can.
  bool dequarantine_ok = RemoveQuarantineAttributes(bundle_path);
  bool prewarm_ok = PrewarmGatekeeperIfSupported(bundle_path) == 0;
  return prewarm_ok && dequarantine_ok;
}

std::optional<base::FilePath> GetWakeTaskPlistPath(UpdaterScope scope) {
  @autoreleasepool {
    NSArray* library_paths = NSSearchPathForDirectoriesInDomains(
        NSLibraryDirectory,
        IsSystemInstall(scope) ? NSLocalDomainMask : NSUserDomainMask, YES);
    if ([library_paths count] < 1) {
      return std::nullopt;
    }
    return base::apple::NSStringToFilePath(library_paths[0])
        .Append(IsSystemInstall(scope) ? "LaunchDaemons" : "LaunchAgents")
        .AppendASCII(base::StrCat({GetWakeLaunchdName(scope), ".plist"}));
  }
}

std::optional<std::string> ReadValueFromPlist(const base::FilePath& path,
                                              const std::string& key) {
  if (key.empty() || path.empty()) {
    return std::nullopt;
  }
  NSData* data;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    data =
        [NSData dataWithContentsOfFile:base::apple::FilePathToNSString(path)];
  }
  if ([data length] == 0) {
    return std::nullopt;
  }
  NSDictionary* all_keys = base::apple::ObjCCastStrict<NSDictionary>(
      [NSPropertyListSerialization propertyListWithData:data
                                                options:NSPropertyListImmutable
                                                 format:nil
                                                  error:nil]);
  if (all_keys == nil) {
    return std::nullopt;
  }
  CFStringRef value = base::apple::GetValueFromDictionary<CFStringRef>(
      base::apple::NSToCFPtrCast(all_keys),
      base::SysUTF8ToCFStringRef(key).get());
  if (value == nullptr) {
    return std::nullopt;
  }
  return base::SysCFStringRefToUTF8(value);
}

bool MigrateLegacyUpdaters(
    UpdaterScope scope,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  return MigrateKeystoneApps(GetKeystoneFolderPath(scope).value(),
                             register_callback);
}

std::optional<base::FilePath> GetBundledEnterpriseCompanionExecutablePath(
    UpdaterScope scope) {
  std::optional<base::FilePath> path = GetUpdaterAppBundlePath(scope);
  if (!path) {
    return std::nullopt;
  }
  return path->Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("Helpers"))
      .Append(base::StrCat({BROWSER_NAME_STRING, "EnterpriseCompanion",
                            kExecutableSuffix, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .AppendASCII(base::StrCat(
          {BROWSER_NAME_STRING, "EnterpriseCompanion", kExecutableSuffix}));
}

}  // namespace updater
