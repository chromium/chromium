// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/util/mac_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#elif BUILDFLAG(IS_WIN)
#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {

namespace {

#if BUILDFLAG(IS_MAC)
base::FilePath AppIDToPath(const std::string& app_id) {
  absl::optional<base::FilePath> application_support_dir =
      GetApplicationSupportDirectory(UpdaterScope::kUser);
  EXPECT_TRUE(application_support_dir);
  return (*application_support_dir)
      .Append(FILE_PATH_LITERAL(COMPANY_SHORTNAME_STRING))
      .AppendASCII(base::StrCat({"UpdateUsageStatsTaskTest_", app_id}));
}
#endif

void ClearAppUsageStats(const std::string& app_id, UpdaterScope scope) {
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(base::DeletePathRecursively(AppIDToPath(app_id)));
#elif BUILDFLAG(IS_WIN)
  LONG outcome =
      base::win::RegKey(
          UpdaterScopeToHKeyRoot(scope),
          IsSystemInstall(scope) ? CLIENT_STATE_MEDIUM_KEY : CLIENT_STATE_KEY,
          Wow6432(KEY_WRITE))
          .DeleteKey(base::SysUTF8ToWide(app_id).c_str());
  ASSERT_TRUE(outcome == ERROR_SUCCESS || outcome == ERROR_FILE_NOT_FOUND);
#endif
}

}  // namespace

class UpdateUsageStatsTaskTest : public testing::Test {
 protected:
  void SetAppUsageStats(const std::string& app_id, bool enabled) {
    cleanups_.emplace_back(
        base::BindOnce(&ClearAppUsageStats, app_id, GetTestScope()));
#if BUILDFLAG(IS_MAC)
    base::CreateDirectory(AppIDToPath(app_id));
    std::unique_ptr<crashpad::CrashReportDatabase> database =
        crashpad::CrashReportDatabase::Initialize(
            AppIDToPath(app_id).AppendASCII("Crashpad"));
    ASSERT_TRUE(database);
    database->GetSettings()->SetUploadsEnabled(enabled);
#elif BUILDFLAG(IS_WIN)
    base::win::RegKey key = base::win::RegKey(
        UpdaterScopeToHKeyRoot(GetTestScope()),
        IsSystemInstall(GetTestScope()) ? CLIENT_STATE_MEDIUM_KEY
                                        : CLIENT_STATE_KEY,
        Wow6432(KEY_WRITE));
    ASSERT_EQ(
        key.CreateKey(base::SysUTF8ToWide(app_id).c_str(), Wow6432(KEY_WRITE)),
        ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(L"usagestats", enabled ? 1 : 0), ERROR_SUCCESS);
#endif
  }

 private:
  std::vector<base::ScopedClosureRunner> cleanups_;
};

// Mac Google-branded builds may pick up Chrome or other Google software
// usagestat opt-ins from outside this test. Disable the test in that
// configuration.
#if !BUILDFLAG(IS_MAC) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(UpdateUsageStatsTaskTest, NoApps) {
  ClearAppUsageStats("app1", GetTestScope());
  ClearAppUsageStats("app2", GetTestScope());
  ASSERT_FALSE(OtherAppUsageStatsAllowed({"app1", "app2"}, GetTestScope()));
}

// TODO(crbug.com/1367437): Enable tests once updater is implemented for Linux
#if !BUILDFLAG(IS_LINUX)
TEST_F(UpdateUsageStatsTaskTest, OneAppEnabled) {
  SetAppUsageStats("app1", true);
  SetAppUsageStats("app2", false);
  ASSERT_TRUE(OtherAppUsageStatsAllowed({"app1", "app2"}, GetTestScope()));
}
#endif  // !BUILDFLAG(IS_LINUX)

TEST_F(UpdateUsageStatsTaskTest, ZeroAppsEnabled) {
  SetAppUsageStats("app1", false);
  SetAppUsageStats("app2", false);
  ASSERT_FALSE(OtherAppUsageStatsAllowed({"app1", "app2"}, GetTestScope()));
}
#endif

}  // namespace updater
