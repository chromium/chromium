// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/installer.h"

namespace updater {

AppInstallerResult RunApplicationInstaller(
    const AppInfo& app_info,
    const base::FilePath& installer_path,
    const std::string& arguments,
    const absl::optional<base::FilePath>& install_data_file,
    bool usage_stats_enabled,
    const base::TimeDelta& timeout,
    InstallProgressCallback /*progress_callback*/) {
  base::LaunchOptions options;
  if (install_data_file) {
    options.environment.emplace(base::ToUpperASCII(kInstallerDataSwitch),
                                install_data_file->value());
  }

  base::SetPosixFilePermissions(installer_path,
                                base::FILE_PERMISSION_USER_MASK |
                                    base::FILE_PERMISSION_GROUP_MASK |
                                    base::FILE_PERMISSION_READ_BY_OTHERS |
                                    base::FILE_PERMISSION_EXECUTE_BY_OTHERS);

  base::CommandLine command(installer_path);
  std::vector<std::string> arg_vec =
      base::SplitString(arguments, base::kWhitespaceASCII,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& arg : arg_vec) {
    command.AppendArg(arg);
  }

  int exit_code = 0;
  if (!base::LaunchProcess(command, options)
           .WaitForExitWithTimeout(timeout, &exit_code)) {
    LOG(ERROR) << "Could not launch application installer.";
    return AppInstallerResult(kErrorApplicationInstallerFailed,
                              kErrorProcessLaunchFailed);
  }
  if (exit_code != 0) {
    LOG(ERROR) << "Installer returned error code " << exit_code;
    return AppInstallerResult(kErrorApplicationInstallerFailed, exit_code);
  }

  return AppInstallerResult();
}

}  // namespace updater
