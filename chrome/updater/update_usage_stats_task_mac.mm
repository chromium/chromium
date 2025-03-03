// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace updater {

namespace {

bool AnyAppUsageStatsAllowedInDir(const base::FilePath& base_dir) {
  base::FileEnumerator files(
      base_dir.Append(FILE_PATH_LITERAL(COMPANY_SHORTNAME_STRING)), false,
      base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath name = files.Next(); !name.empty(); name = files.Next()) {
    if (name.BaseName().value() == PRODUCT_FULLNAME_STRING) {
      continue;
    }
    base::FilePath crashpad = name.Append("Crashpad");
    if (base::PathExists(crashpad)) {
      std::unique_ptr<crashpad::CrashReportDatabase> app_database =
          crashpad::CrashReportDatabase::Initialize(crashpad);
      if (!app_database) {
        continue;
      }
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
bool AnyAppUsageStatsAllowed(UpdaterScope scope) {
  if (!IsSystemInstall(scope)) {
    std::optional<base::FilePath> application_support_dir =
        GetApplicationSupportDirectory(UpdaterScope::kUser);
    return application_support_dir &&
           AnyAppUsageStatsAllowedInDir(*application_support_dir);
  }
  // In the system case, iterate all users. If any user has opted-in to usage
  // stats, the system updater may transmit usage stats.
  base::FilePath user_dir;
  if (!base::apple::GetLocalDirectory(NSUserDirectory, &user_dir)) {
    return false;
  }
  base::FileEnumerator files(user_dir, false,
                             base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath name = files.Next(); !name.empty(); name = files.Next()) {
    if (AnyAppUsageStatsAllowedInDir(
            name.Append(FILE_PATH_LITERAL("Library"))
                .Append(FILE_PATH_LITERAL("Application Support")))) {
      return true;
    }
  }
  return false;
}

void UpdateUsageStatsTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AnyAppUsageStatsAllowed, scope_),
      base::BindOnce(&UpdateUsageStatsTask::SetUsageStatsEnabled, this,
                     persisted_data_)
          .Then(std::move(callback)));
}

}  // namespace updater
