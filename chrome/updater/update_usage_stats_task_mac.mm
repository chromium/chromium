// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace updater {

namespace {

bool UsageStatsAllowedInDir(const base::FilePath& base_dir) {
  base::FileEnumerator files(
      base_dir.Append(FILE_PATH_LITERAL(COMPANY_SHORTNAME_STRING)), false,
      base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath name = files.Next(); !name.empty(); name = files.Next()) {
    if (name.BaseName().value() == PRODUCT_FULLNAME_STRING) {
      continue;
    }
    base::FilePath crashpad = name.AppendASCII("Crashpad");
    if (base::PathExists(crashpad)) {
      std::unique_ptr<crashpad::CrashReportDatabase> app_database =
          crashpad::CrashReportDatabase::Initialize(crashpad);
      if (!app_database)
        continue;
      crashpad::Settings* app_settings = app_database->GetSettings();
      bool enabled = false;
      if (app_settings->GetUploadsEnabled(&enabled) && enabled) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

// Returns whether any crashpad databases in ~/Library/Application
// Support/<company name>/<app name>/Crashpad have upload enabled. Google
// Chrome channels all follow this pattern.
bool UpdateUsageStatsTask::UsageStatsAllowed(
    const std::vector<std::string>& app_ids) const {
  if (!IsSystemInstall(scope_)) {
    absl::optional<base::FilePath> application_support_dir =
        GetApplicationSupportDirectory(UpdaterScope::kUser);
    return application_support_dir &&
           UsageStatsAllowedInDir(*application_support_dir);
  }
  // In the system case, iterate all users. If any user has opted-in to usage
  // stats, the system updater may transmit usage stats.
  base::FilePath user_dir;
  if (!base::mac::GetLocalDirectory(NSUserDirectory, &user_dir)) {
    return false;
  }
  base::FileEnumerator files(user_dir, false,
                             base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath name = files.Next(); !name.empty(); name = files.Next()) {
    if (UsageStatsAllowedInDir(
            name.Append(FILE_PATH_LITERAL("Library"))
                .Append(FILE_PATH_LITERAL("Application Support")))) {
      return true;
    }
  }
  return false;
}

}  // namespace updater
