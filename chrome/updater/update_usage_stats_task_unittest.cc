// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if BUILDFLAG(IS_MAC)
#include "base/files/scoped_temp_dir.h"
#include "chrome/updater/util/mac_util.h"
#elif BUILDFLAG(IS_WIN)
#include "base/strings/sys_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {

class UpdateUsageStatsTaskTest : public testing::Test {
  protected:
#if BUILDFLAG(IS_MAC)
  void SetAppUsageStats(const std::string& app_id,
                        bool enabled,
                        UpdaterScope scope) {
    base::FilePath install_dir = IsSystemInstall(scope)
                                     ? fake_system_directory_.GetPath()
                                     : fake_user_directory_.GetPath();
    base::FilePath app_dir = install_dir.Append(app_id);

    ASSERT_TRUE(base::CreateDirectory(app_dir));
    std::unique_ptr<crashpad::CrashReportDatabase> database =
        crashpad::CrashReportDatabase::Initialize(app_dir.Append("Crashpad"));
    ASSERT_TRUE(database &&
                database->GetSettings()->SetUploadsEnabled(enabled));
  }

  void SetUp() override {
    ASSERT_TRUE(fake_user_directory_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_system_directory_.CreateUniqueTempDir());
    std::vector<base::FilePath> install_directories(
        {fake_user_directory_.GetPath()});
    if (IsSystemInstall(scope_)) {
      install_directories.push_back(fake_system_directory_.GetPath());
    }
    usage_stats_provider_ = UsageStatsProvider::Create(install_directories);
  }

#elif BUILDFLAG(IS_WIN)
  void SetAppUsageStats(const std::string& app_id,
                        bool enabled,
                        UpdaterScope scope) {
    std::wstring path =
        IsSystemInstall(scope) ? system_key_path_ : user_key_path_;
    base::win::RegKey key;
    ASSERT_EQ(key.Open(hive_, path.c_str(), Wow6432(KEY_WRITE)), ERROR_SUCCESS);
    ASSERT_EQ(
        key.CreateKey(base::SysUTF8ToWide(app_id).c_str(), Wow6432(KEY_WRITE)),
        ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(L"usagestats", enabled ? 1 : 0), ERROR_SUCCESS);
  }

  void SetUp() override {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(hive_, user_key_path_.c_str(), Wow6432(KEY_WRITE)),
              ERROR_SUCCESS);
    ASSERT_EQ(key.Create(hive_, system_key_path_.c_str(), Wow6432(KEY_WRITE)),
              ERROR_SUCCESS);
    std::vector<std::wstring> key_paths({user_key_path_});
    if (IsSystemInstall(scope_)) {
      key_paths.push_back(system_key_path_);
    }
    usage_stats_provider_ = UsageStatsProvider::Create(hive_, key_paths);
  }

  void TearDown() override {
    for (const std::wstring& key_path :
         std::vector<std::wstring>({user_key_path_, system_key_path_})) {
      LONG result = base::win::RegKey(hive_, key_path.c_str(), Wow6432(DELETE))
                        .DeleteKey(L"");
      EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
                  result == ERROR_INVALID_HANDLE);
    }
  }
#endif  // BUILDFLAG(IS_WIN)
  std::unique_ptr<UsageStatsProvider> usage_stats_provider_;
  UpdaterScope scope_ = GetUpdaterScopeForTesting();

 private:
#if BUILDFLAG(IS_MAC)
  base::ScopedTempDir fake_user_directory_;
  base::ScopedTempDir fake_system_directory_;
#elif BUILDFLAG(IS_WIN)
  HKEY hive_ = UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting());
  std::wstring user_key_path_ =
      base::StrCat({UPDATER_KEY, L"UsageStatsProviderTestUserKey\\"});
  std::wstring system_key_path_ =
      base::StrCat({UPDATER_KEY, L"UsageStatsProviderTestSystemkey\\"});
#endif
};

#if BUILDFLAG(IS_LINUX)
TEST_F(UpdateUsageStatsTaskTest, LinuxAlwaysFalse) {
  ASSERT_FALSE(UsageStatsProvider::Create(scope_)->AnyAppEnablesUsageStats());
}
#else

TEST_F(UpdateUsageStatsTaskTest, NoApps) {
  ASSERT_FALSE(usage_stats_provider_->AnyAppEnablesUsageStats());
}

TEST_F(UpdateUsageStatsTaskTest, OneAppDisabled) {
  SetAppUsageStats("app1", false, scope_);
  SetAppUsageStats("app2", false, scope_);
  ASSERT_FALSE(usage_stats_provider_->AnyAppEnablesUsageStats());
}

TEST_F(UpdateUsageStatsTaskTest, OneAppEnabled) {
  SetAppUsageStats("app1", true, scope_);
  SetAppUsageStats("app2", false, scope_);
  ASSERT_TRUE(usage_stats_provider_->AnyAppEnablesUsageStats());
}

TEST_F(UpdateUsageStatsTaskTest, UserInstallIgnoresSystem) {
  if (IsSystemInstall(scope_)) {
    return;
  }
  SetAppUsageStats("app1", false, UpdaterScope::kUser);
  SetAppUsageStats("app1", true, UpdaterScope::kSystem);
  ASSERT_FALSE(usage_stats_provider_->AnyAppEnablesUsageStats());
}

TEST_F(UpdateUsageStatsTaskTest, SystemInstallLooksAtUser) {
  if (!IsSystemInstall(scope_)) {
    return;
  }
  SetAppUsageStats("app1", true, UpdaterScope::kUser);
  SetAppUsageStats("app1", false, UpdaterScope::kSystem);
  ASSERT_TRUE(usage_stats_provider_->AnyAppEnablesUsageStats());
}

#endif

}  // namespace updater
