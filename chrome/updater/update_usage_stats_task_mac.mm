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
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace updater {

namespace {

// Returns the Application Support directories associated with the given scope.
// These directories are located under
// /Users/<user>/Library/Application\ Support. Returns the directory for all
// users in the system case or the current user's otherwise.
std::vector<base::FilePath> GetAppSupportDirectoriesForScope(
    UpdaterScope scope) {
  std::vector<base::FilePath> app_support_dirs;
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
  } else {
    if (std::optional<base::FilePath> application_support_dir =
            GetApplicationSupportDirectory(UpdaterScope::kUser);
        application_support_dir) {
      app_support_dirs.push_back(*application_support_dir);
    }
  }
  return app_support_dirs;
}

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
  explicit UsageStatsProviderImpl(const base::FilePath& install_directory)
      : install_directory_(install_directory) {}

  // Returns true if any app directory under
  // "Application Support/<install_directory_>" for the given scope has
  // usage stats enabled.
  bool AnyAppEnablesUsageStats(UpdaterScope scope) override {
    return std::ranges::any_of(
        GetAppDirectories(scope), [](const base::FilePath& app_dir) {
          return app_dir.BaseName().value() != PRODUCT_FULLNAME_STRING &&
                 AppAllowsUsageStats(app_dir);
        });
  }

  std::vector<base::FilePath> GetAppDirectories(UpdaterScope scope) {
    std::vector<base::FilePath> all_apps;
    for (const base::FilePath& app_support_dir :
         GetAppSupportDirectoriesForScope(scope)) {
      base::FileEnumerator(app_support_dir.Append(install_directory_),
                           /*recursive=*/false,
                           base::FileEnumerator::FileType::DIRECTORIES)
          .ForEach([&all_apps](const base::FilePath& app) {
            all_apps.push_back(app);
          });
    }
    return all_apps;
  }

 private:
  base::FilePath install_directory_;
};

// Returns a UsageStatsProvider that checks usage stats opt in for apps found
// under "Application Support/<COMPANY_NAME>." Google Chrome channels all follow
// this pattern.
std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create() {
  return UsageStatsProvider::Create(base::FilePath(COMPANY_SHORTNAME_STRING));
}

std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create(
    const base::FilePath& install_directory) {
  return std::make_unique<UsageStatsProviderImpl>(install_directory);
}

}  // namespace updater
