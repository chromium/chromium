// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/install_from_archive.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"

namespace updater {

InstallerResult RunApplicationInstaller(
    const AppInfo& app_info,
    const base::FilePath& app_installer,
    const std::string& arguments,
    const std::optional<base::FilePath>& installer_data_file,
    bool usage_stats_enabled,
    base::TimeDelta timeout,
    InstallProgressCallback /*progress_callback*/) {
  if (!PrepareToRunBundle(app_installer)) {
    VLOG(0) << "Prep failed -- Gatekeeper may prompt for " << app_installer;
  }

  VLOG(1) << "Running application install at " << app_installer;
  // InstallFromArchive() returns the exit code of the script. 0 is success and
  // anything else should be an error.
  const int exit_code =
      InstallFromArchive(app_installer, app_info.ecp, app_info.ap,
                         app_info.scope, app_info.version, arguments,
                         installer_data_file, usage_stats_enabled, timeout);
  return exit_code == 0
             ? InstallerResult()
             : InstallerResult(kErrorApplicationInstallerFailed, exit_code);
}

std::string LookupString(const base::FilePath& path,
                         const std::string& keyname,
                         const std::string& default_value) {
  std::optional<std::string> value = ReadValueFromPlist(path, keyname);
  return value ? *value : default_value;
}

base::Version LookupVersion(UpdaterScope scope,
                            const std::string& app_id,
                            const base::FilePath& version_path,
                            const std::string& version_key,
                            const base::Version& default_value) {
  std::optional<std::string> value =
      ReadValueFromPlist(version_path, version_key);
  if (value) {
    base::Version value_version(*value);
    return value_version.IsValid() ? value_version : default_value;
  }
  return default_value;
}

}  // namespace updater
