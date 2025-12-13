// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/usage_stats_permissions.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/process/process_iterator.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace updater {

namespace {

// Returns true if the directory contains a crashpad database with uploads
// enabled.
bool AppAllowsUsageStats(const base::FilePath& app_directory) {
  bool enabled = false;
  return crashpad::CrashReportDatabase::GetSettingsReaderForDatabasePath(
             app_directory.Append("Crashpad"))
             ->GetUploadsEnabled(&enabled) &&
         enabled;
}

std::vector<base::FilePath> GetAppDirectories(
    const std::vector<base::FilePath>& app_support_directories) {
  std::vector<base::FilePath> all_apps;
  for (const base::FilePath& app_support_directory : app_support_directories) {
    base::FileEnumerator(app_support_directory.Append(COMPANY_SHORTNAME_STRING),
                         /*recursive=*/false,
                         base::FileEnumerator::FileType::DIRECTORIES)
        .ForEach([&all_apps](const base::FilePath& app) {
          all_apps.push_back(app);
        });
  }
  return all_apps;
}

}  // namespace

// Returns true if any app directory under the associated
// `app_support_directories` has usage stats enabled.
bool AnyAppEnablesUsageStats(
    const std::vector<base::FilePath>& app_support_directories) {
  return std::ranges::any_of(GetAppDirectories(app_support_directories),
                             [](const base::FilePath& app_dir) {
                               return app_dir.BaseName().value() !=
                                          PRODUCT_FULLNAME_STRING &&
                                      AppAllowsUsageStats(app_dir);
                             });
}

bool RemoteEventLoggingAllowed(
    const std::vector<std::string>& installed_app_ids,
    const std::vector<base::FilePath>& app_support_directories,
    std::optional<EventLoggingPermissionProvider>
        event_logging_permission_provider) {
  if (!event_logging_permission_provider) {
    VLOG(2) << "Event logging disabled by absence of permission provider";
    return false;
  }

  bool manages_additional_apps =
      std::ranges::any_of(installed_app_ids, [&](const std::string& app_id) {
        return !IsRemoteEventLoggingPermissionExempt(app_id) &&
               !base::EqualsCaseInsensitiveASCII(
                   app_id, event_logging_permission_provider->app_id);
      });

  if (manages_additional_apps) {
    VLOG(2) << "Event logging disabled by presence of other apps";
    return false;
  }

  bool allowed = std::ranges::any_of(
      app_support_directories,
      [&](const base::FilePath& app_support_directory) {
        return AppAllowsUsageStats(
            app_support_directory.Append(COMPANY_SHORTNAME_STRING)
                .Append(event_logging_permission_provider->directory_name));
      });

  VLOG_IF(2, !allowed) << "Event logging disabled; app "
                       << event_logging_permission_provider->app_id
                       << " does not enable usage stats";
  return allowed;
}

bool AnyAppEnablesUsageStats(UpdaterScope scope) {
  return AnyAppEnablesUsageStats(
      GetApplicationSupportDirectoriesForScope(scope));
}

bool RemoteEventLoggingAllowed(
    UpdaterScope scope,
    const std::vector<std::string>& installed_app_ids,
    std::optional<EventLoggingPermissionProvider>
        event_logging_permission_provider) {
  return RemoteEventLoggingAllowed(
      installed_app_ids, GetApplicationSupportDirectoriesForScope(scope),
      std::move(event_logging_permission_provider));
}

}  // namespace updater
