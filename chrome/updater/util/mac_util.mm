// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/util/mac_util.h"

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

constexpr int kLaunchctlExitCodeNoSuchProcess = 3;

constexpr base::FilePath::CharType kZipExePath[] =
    FILE_PATH_LITERAL("/usr/bin/unzip");

base::FilePath ExecutableFolderPath() {
  return base::FilePath(
             base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"));
}

}  // namespace

absl::optional<base::FilePath> GetLibraryFolderPath(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return base::mac::GetUserLibraryPath();
    case UpdaterScope::kSystem: {
      base::FilePath local_library_path;
      if (!base::mac::GetLocalDirectory(NSLibraryDirectory,
                                        &local_library_path)) {
        VLOG(1) << "Could not get local library path";
        return absl::nullopt;
      }
      return local_library_path;
    }
  }
}

absl::optional<base::FilePath> GetApplicationSupportDirectory(
    UpdaterScope scope) {
  base::FilePath path;
  switch (scope) {
    case UpdaterScope::kUser:
      if (base::mac::GetUserDirectory(NSApplicationSupportDirectory, &path))
        return path;
      break;
    case UpdaterScope::kSystem:
      if (base::mac::GetLocalDirectory(NSApplicationSupportDirectory, &path))
        return path;
      break;
  }

  VLOG(1) << "Could not get applications support path";
  return absl::nullopt;
}

absl::optional<base::FilePath> GetKSAdminPath(UpdaterScope scope) {
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

base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName(UpdaterScope scope) {
  return base::SysUTF8ToCFStringRef(
      IsSystemInstall(scope) ? MAC_BUNDLE_IDENTIFIER_STRING ".wake.system"
                             : MAC_BUNDLE_IDENTIFIER_STRING ".wake");
}

bool RemoveJobFromLaunchd(UpdaterScope scope,
                          Launchd::Domain domain,
                          Launchd::Type type,
                          base::ScopedCFTypeRef<CFStringRef> name) {
  // This may block while deleting the launchd plist file.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (Launchd::GetInstance()->PlistExists(domain, type, name)) {
    if (!Launchd::GetInstance()->DeletePlist(domain, type, name))
      return false;
  }

  base::CommandLine command_line(base::FilePath("/bin/launchctl"));
  command_line.AppendArg("remove");
  command_line.AppendArg(base::SysCFStringRefToUTF8(name));
  if (IsSystemInstall(scope))
    command_line = MakeElevated(command_line);

  int exit_code = -1;
  std::string output;
  base::GetAppOutputWithExitCode(command_line, &output, &exit_code);
  return exit_code == 0 || exit_code == kLaunchctlExitCodeNoSuchProcess;
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

absl::optional<base::FilePath> GetExecutableFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version) {
  absl::optional<base::FilePath> path =
      GetVersionedInstallDirectory(scope, version);
  if (!path)
    return absl::nullopt;
  return path->Append(ExecutableFolderPath());
}

absl::optional<base::FilePath> GetUpdaterAppBundlePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetVersionedInstallDirectory(scope);
  if (!path)
    return absl::nullopt;
  return path->Append(
      base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"}));
}

base::FilePath GetExecutableRelativePath() {
  return ExecutableFolderPath().Append(
      base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix}));
}

absl::optional<base::FilePath> GetKeystoneFolderPath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  if (!path)
    return absl::nullopt;
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
      if (!ConfirmFilePermissions(path, kPermissionsMask))
        return false;
    }
  }

  return true;
}

absl::optional<base::FilePath> GetBaseInstallDirectory(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  return path ? absl::optional<base::FilePath>(
                    path->Append(GetUpdaterFolderName()))
              : absl::nullopt;
}

absl::optional<base::FilePath> GetUpdateServiceLauncherPath(
    UpdaterScope scope) {
  absl::optional<base::FilePath> install_dir = GetBaseInstallDirectory(scope);
  return install_dir
             ? absl::optional<base::FilePath>(
                   install_dir->Append("Current")
                       .Append(base::StrCat({PRODUCT_FULLNAME_STRING,
                                             kExecutableSuffix, ".app"}))
                       .Append("Contents")
                       .Append("Helpers")
                       .Append("launcher"))
             : absl::nullopt;
}

bool RemoveQuarantineAttributes(const base::FilePath& updater_bundle_path) {
  bool success = base::mac::RemoveQuarantineAttribute(updater_bundle_path);
  base::FileEnumerator file_enumerator(
      base::FilePath(updater_bundle_path), true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    success = base::mac::RemoveQuarantineAttribute(name) && success;
  }
  return success;
}

}  // namespace updater
