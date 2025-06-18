// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/util/mac_util.h"

#import <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>
#include <vector>

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

constexpr base::FilePath::CharType kGkToolPath[] =
    FILE_PATH_LITERAL("/usr/bin/gktool");

base::FilePath ExecutableFolderPath() {
  return base::FilePath(
             base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"));
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

bool RemoveQuarantineAttributes(const base::FilePath& path) {
  bool success = base::mac::RemoveQuarantineAttribute(path);
  base::FileEnumerator file_enumerator(
      base::FilePath(path), true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    success = base::mac::RemoveQuarantineAttribute(name) && success;
  }
  return success;
}

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

std::vector<base::FilePath> GetApplicationSupportDirectoriesForScope(
    UpdaterScope scope) {
  std::vector<base::FilePath> app_support_dirs;
  std::optional<base::FilePath> application_support_dir =
      GetApplicationSupportDirectory(scope);
  if (application_support_dir) {
    app_support_dirs.push_back(*application_support_dir);
  }
  if (IsSystemInstall(scope)) {
    base::FilePath user_dir;
    if (!base::apple::GetLocalDirectory(NSUserDirectory, &user_dir)) {
      return {};
    }
    base::FileEnumerator(user_dir, /*recursive=*/false,
                         base::FileEnumerator::FileType::DIRECTORIES)
        .ForEach([&app_support_dirs](const base::FilePath& name) {
          app_support_dirs.push_back(
              name.Append("Library").Append("Application Support"));
        });
  }
  return app_support_dirs;
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

bool SetFilePermissionsRecursive(const base::FilePath& path) {
  static constexpr mode_t executable_mode =
      S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  static constexpr mode_t normal_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  struct stat stat_buf;
  if (lstat(path.value().c_str(), &stat_buf) != 0) {
    VPLOG(2) << "Couldn't stat: " << path.value();
    return false;
  }
  if (lchmod(path.value().c_str(),
             (stat_buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH) ||
              S_ISDIR(stat_buf.st_mode))
                 ? executable_mode
                 : normal_mode) != 0) {
    VPLOG(2) << "Couldn't set file permissions for: " << path.value();
    return S_ISLNK(stat_buf.st_mode);  // Tolerate failures on symbolic links.
  }
  if (S_ISDIR(stat_buf.st_mode)) {
    base::FileEnumerator file_enumerator(path, false,
                                         base::FileEnumerator::NAMES_ONLY);
    for (base::FilePath child_path = file_enumerator.Next();
         !child_path.empty(); child_path = file_enumerator.Next()) {
      if (!SetFilePermissionsRecursive(child_path)) {
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
        .Append(base::StrCat({GetWakeLaunchdName(scope), ".plist"}));
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
      .Append(base::StrCat(
          {BROWSER_NAME_STRING, "EnterpriseCompanion", kExecutableSuffix}));
}

}  // namespace updater
