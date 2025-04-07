// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/enterprise_companion/installer_paths.h"
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
  base::FilePath crashpad = app_directory.Append("Crashpad");
  if (!base::PathExists(crashpad)) {
    return false;
  }
  std::unique_ptr<crashpad::CrashReportDatabase> app_database =
      crashpad::CrashReportDatabase::Initialize(crashpad);
  if (!app_database) {
    return false;
  }
  bool enabled = false;
  return app_database->GetSettings()->GetUploadsEnabled(&enabled) && enabled;
}

}  // namespace

class UsageStatsProviderImpl : public UsageStatsProvider {
 public:
  explicit UsageStatsProviderImpl(
      std::optional<std::string> event_logging_permission_provider,
      std::vector<base::FilePath> install_directories)
      : event_logging_permission_provider_(
            std::move(event_logging_permission_provider)),
        install_directories_(std::move(install_directories)) {}

  // Returns true if any app directory under the associated
  // `install_directories` has usage stats enabled.
  bool AnyAppEnablesUsageStats() const override {
    return std::ranges::any_of(
        GetAppDirectories(), [](const base::FilePath& app_dir) {
          return app_dir.BaseName().value() != PRODUCT_FULLNAME_STRING &&
                 AppAllowsUsageStats(app_dir);
        });
  }

  std::vector<base::FilePath> GetAppDirectories() const {
    std::vector<base::FilePath> all_apps;
    for (const base::FilePath& install_dir : install_directories_) {
      base::FileEnumerator(install_dir,
                           /*recursive=*/false,
                           base::FileEnumerator::FileType::DIRECTORIES)
          .ForEach([&all_apps](const base::FilePath& app) {
            all_apps.push_back(app);
          });
    }
    return all_apps;
  }

  bool RemoteEventLoggingAllowed() const override {
    if (!event_logging_permission_provider_) {
      return false;
    }

    if (std::ranges::any_of(
            GetAppDirectories(), [this](const base::FilePath& app_dir) {
              std::string name = app_dir.BaseName().value();
              std::optional<base::FilePath> enterprise_companion_app_path =
                  enterprise_companion::GetInstallDirectory();
              return name != PRODUCT_FULLNAME_STRING &&
                     name != event_logging_permission_provider_ &&
                     (!enterprise_companion_app_path ||
                      name !=
                          enterprise_companion_app_path->BaseName().value());
            })) {
      return false;
    }

    return std::ranges::any_of(
        install_directories_, [this](const base::FilePath& install_dir) {
          return AppAllowsUsageStats(
              install_dir.Append(*event_logging_permission_provider_));
        });
  }

 private:
  std::optional<std::string> event_logging_permission_provider_;
  std::vector<base::FilePath> install_directories_;
};

// Returns a UsageStatsProvider that checks usage stats opt in for apps found
// under "Application Support/<COMPANY_NAME>." Google Chrome channels all follow
// this pattern.
std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create(
    UpdaterScope scope) {
  return UsageStatsProvider::Create(
      std::nullopt, GetApplicationSupportDirectoriesForUsers(scope));
}

std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create(
    std::optional<std::string> event_logging_permission_provider,
    std::vector<base::FilePath> install_directories) {
  return std::make_unique<UsageStatsProviderImpl>(
      std::move(event_logging_permission_provider),
      std::move(install_directories));
}

}  // namespace updater
