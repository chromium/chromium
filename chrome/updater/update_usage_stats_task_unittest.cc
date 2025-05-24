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
#include "chrome/updater/branded_constants.h"
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
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/updater/util/mac_util.h"
#elif BUILDFLAG(IS_WIN)
#include "base/strings/sys_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {

class UpdateUsageStatsTaskTest : public testing::Test {
 protected:
  std::string fake_permission_provider_ = "UsageStatsTestPermissionProvider";
#if BUILDFLAG(IS_MAC)
  base::ScopedTempDir fake_user_directory_;
  base::ScopedTempDir fake_system_directory_;

  std::unique_ptr<UsageStatsProvider>
  UsageStatsProviderWithNullPermissionProvider() {
    return UsageStatsProvider::Create(std::nullopt, InstallDirectories());
  }

  void SetUpdaterUsageStats(bool enabled, UpdaterScope scope) {
    SetAppUsageStats(PRODUCT_FULLNAME_STRING, enabled, scope);
  }

  void SetCECAUsageStats(bool enabled, UpdaterScope scope) {
    std::optional<base::FilePath> ceca_path =
        enterprise_companion::GetInstallDirectory();
    ASSERT_TRUE(ceca_path);
    SetAppUsageStats(ceca_path->BaseName().value(), enabled, scope);
  }

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
    usage_stats_provider_ = UsageStatsProvider::Create(
        fake_permission_provider_, InstallDirectories());
  }

#elif BUILDFLAG(IS_WIN)

  std::unique_ptr<UsageStatsProvider>
  UsageStatsProviderWithNullPermissionProvider() {
    return UsageStatsProvider::Create(hive_, std::nullopt,
                                      InstallRegistryPaths());
  }

  void SetUpdaterUsageStats(bool enabled, UpdaterScope scope) {
    SetAppUsageStats(kUpdaterAppId, enabled, scope);
  }

  void SetCECAUsageStats(bool enabled, UpdaterScope scope) {
    SetAppUsageStats(enterprise_companion::kCompanionAppId, enabled, scope);
  }
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
    usage_stats_provider_ = UsageStatsProvider::Create(
        hive_, base::UTF8ToWide(fake_permission_provider_),
        InstallRegistryPaths());
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
  std::vector<base::FilePath> InstallDirectories() {
    std::vector<base::FilePath> install_directories(
        {fake_user_directory_.GetPath()});
    if (IsSystemInstall(scope_)) {
      install_directories.push_back(fake_system_directory_.GetPath());
    }
    return install_directories;
  }
#elif BUILDFLAG(IS_WIN)

  std::vector<std::wstring> InstallRegistryPaths() {
    std::vector<std::wstring> key_paths({user_key_path_});
    if (IsSystemInstall(scope_)) {
      key_paths.push_back(system_key_path_);
    }
    return key_paths;
  }
  HKEY hive_ = UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting());
  std::wstring user_key_path_ =
      base::StrCat({UPDATER_KEY, L"UsageStatsProviderTestUserKey\\"});
  std::wstring system_key_path_ =
      base::StrCat({UPDATER_KEY, L"UsageStatsProviderTestSystemkey\\"});
#endif
};

#if BUILDFLAG(IS_LINUX)
TEST_F(UpdateUsageStatsTaskTest, LinuxAlwaysFalse) {
  std::unique_ptr<UsageStatsProvider> usage_stats_provider =
      UsageStatsProvider::Create(scope_);
  ASSERT_FALSE(usage_stats_provider->AnyAppEnablesUsageStats());
  ASSERT_FALSE(usage_stats_provider->RemoteEventLoggingAllowed());
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
    GTEST_SKIP() << "Not applicable to system-scoped installs";
  }
  SetAppUsageStats("app1", false, UpdaterScope::kUser);
  SetAppUsageStats("app1", true, UpdaterScope::kSystem);
  ASSERT_FALSE(usage_stats_provider_->AnyAppEnablesUsageStats());
}

TEST_F(UpdateUsageStatsTaskTest, SystemInstallLooksAtUser) {
  if (!IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to user-scoped installs";
  }
  SetAppUsageStats("app1", true, UpdaterScope::kUser);
  SetAppUsageStats("app1", false, UpdaterScope::kSystem);
  ASSERT_TRUE(usage_stats_provider_->AnyAppEnablesUsageStats());
}

TEST_F(UpdateUsageStatsTaskTest, NullPermissionProviderReturnsFalse) {
  ASSERT_FALSE(UsageStatsProviderWithNullPermissionProvider()
                   ->RemoteEventLoggingAllowed());
}

TEST_F(UpdateUsageStatsTaskTest, NullPermissionProviderIgnoresAppPermissions) {
  SetAppUsageStats("app1", true, scope_);
  SetAppUsageStats("app2", false, scope_);
  SetAppUsageStats(fake_permission_provider_, true, scope_);
  ASSERT_FALSE(UsageStatsProviderWithNullPermissionProvider()
                   ->RemoteEventLoggingAllowed());
}

TEST_F(UpdateUsageStatsTaskTest, PermissionProviderAllowsRemoteLogging) {
  SetAppUsageStats(fake_permission_provider_, true, scope_);
  ASSERT_TRUE(usage_stats_provider_->RemoteEventLoggingAllowed());
}

TEST_F(UpdateUsageStatsTaskTest,
       PermissionProviderAllowsRemoteLoggingWithCECAAndUpdater) {
  SetUpdaterUsageStats(true, scope_);
  SetCECAUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_, true, scope_);
  ASSERT_TRUE(usage_stats_provider_->RemoteEventLoggingAllowed());
}

TEST_F(UpdateUsageStatsTaskTest, UsageStatsProviderChecksPermissionProvider) {
  SetUpdaterUsageStats(true, scope_);
  SetCECAUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_, false, scope_);
  ASSERT_FALSE(usage_stats_provider_->RemoteEventLoggingAllowed());
}

TEST_F(UpdateUsageStatsTaskTest,
       PermissionProviderDisallowsRemoteLoggingWithOtherAppDisabled) {
  SetUpdaterUsageStats(true, scope_);
  SetCECAUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_, true, scope_);
  SetAppUsageStats("unsupported_app", false, scope_);
  ASSERT_FALSE(usage_stats_provider_->RemoteEventLoggingAllowed());
}

TEST_F(UpdateUsageStatsTaskTest,
       PermissionProviderDisallowsRemoteLoggingWithOtherAppEnabled) {
  SetUpdaterUsageStats(true, scope_);
  SetCECAUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_, true, scope_);
  SetAppUsageStats("unsupported_app", true, scope_);
  ASSERT_FALSE(usage_stats_provider_->RemoteEventLoggingAllowed());
}

TEST_F(UpdateUsageStatsTaskTest,
       SystemPermissionProviderAllowsRemoteLoggingWithUserAppEnabled) {
  if (!IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to user-scoped installs";
  }
  SetUpdaterUsageStats(true, scope_);
  SetCECAUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_, true, UpdaterScope::kUser);
  SetAppUsageStats(fake_permission_provider_, false, UpdaterScope::kSystem);
  ASSERT_TRUE(usage_stats_provider_->RemoteEventLoggingAllowed());
}

#endif

}  // namespace updater
