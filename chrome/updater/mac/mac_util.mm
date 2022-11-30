// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/mac_util.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
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
#include "chrome/updater/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

constexpr int kLaunchctlExitCodeNoSuchProcess = 3;

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
  if (scope == UpdaterScope::kSystem)
    command_line = MakeElevated(command_line);

  int exit_code = -1;
  std::string output;
  base::GetAppOutputWithExitCode(command_line, &output, &exit_code);
  return exit_code == 0 || exit_code == kLaunchctlExitCodeNoSuchProcess;
}

}  // namespace updater
