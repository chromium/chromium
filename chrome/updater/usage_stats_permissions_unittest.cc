// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/usage_stats_permissions.h"

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
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/external_constants.h"
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

class UsageStatsPermissionsTest : public testing::Test {
 protected:
  EventLoggingPermissionProvider fake_permission_provider_ = {
      .app_id = "UsageStatsTestPermissionProvider",
#if BUILDFLAG(IS_MAC)
      .directory_name = "UsageStatsTestPermissionProvider",
#endif
  };

#if BUILDFLAG(IS_MAC)
  base::ScopedTempDir fake_user_directory_;
  base::ScopedTempDir fake_system_directory_;

  void SetAppUsageStats(const std::string& app_id,
                        bool enabled,
                        UpdaterScope scope) {
    base::FilePath app_support_dir = IsSystemInstall(scope)
                                         ? fake_system_directory_.GetPath()
                                         : fake_user_directory_.GetPath();
    base::FilePath app_dir =
        app_support_dir.Append(COMPANY_SHORTNAME_STRING).Append(app_id);

    ASSERT_TRUE(base::CreateDirectory(app_dir));
    std::unique_ptr<crashpad::CrashReportDatabase> database =
        crashpad::CrashReportDatabase::Initialize(app_dir.Append("Crashpad"));
    ASSERT_TRUE(database &&
                database->GetSettings()->SetUploadsEnabled(enabled));
    installed_app_ids_.push_back(app_id);
  }

  void SetUp() override {
    ASSERT_TRUE(fake_user_directory_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_system_directory_.CreateUniqueTempDir());
  }

  bool AnyAppEnablesUsageStats() {
    return ::updater::AnyAppEnablesUsageStats(ApplicationSupportDirectories());
  }

  bool RemoteEventLoggingAllowed() {
    return ::updater::RemoteEventLoggingAllowed(installed_app_ids_,
                                                ApplicationSupportDirectories(),
                                                fake_permission_provider_);
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
    installed_app_ids_.push_back(app_id);
  }

  void SetUp() override {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(hive_, user_key_path_.c_str(), Wow6432(KEY_WRITE)),
              ERROR_SUCCESS);
    ASSERT_EQ(key.Create(hive_, system_key_path_.c_str(), Wow6432(KEY_WRITE)),
              ERROR_SUCCESS);
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

  bool AnyAppEnablesUsageStats() {
    return ::updater::AnyAppEnablesUsageStats(hive_, InstallRegistryPaths());
  }

  bool RemoteEventLoggingAllowed() {
    return ::updater::RemoteEventLoggingAllowed(hive_, InstallRegistryPaths(),
                                                installed_app_ids_,
                                                fake_permission_provider_);
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  void SetExemptAppsUsageStats(bool enabled, UpdaterScope scope) {
    SetAppUsageStats(kUpdaterAppId, enabled, scope);
    SetAppUsageStats(enterprise_companion::kCompanionAppId, enabled, scope);
    SetAppUsageStats(kPlatformExperienceHelperAppId, enabled, scope);
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  UpdaterScope scope_ = GetUpdaterScopeForTesting();

 private:
#if BUILDFLAG(IS_MAC)
  std::vector<base::FilePath> ApplicationSupportDirectories() {
    std::vector<base::FilePath> application_support_directories(
        {fake_user_directory_.GetPath()});
    if (IsSystemInstall(scope_)) {
      application_support_directories.push_back(
          fake_system_directory_.GetPath());
    }
    return application_support_directories;
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

  std::vector<std::string> installed_app_ids_;
};

#if BUILDFLAG(IS_LINUX)
TEST_F(UsageStatsPermissionsTest, LinuxAlwaysFalse) {
  ASSERT_FALSE(AnyAppEnablesUsageStats(scope_));
  ASSERT_FALSE(
      RemoteEventLoggingAllowed(scope_, {}, fake_permission_provider_));
}
#else

TEST_F(UsageStatsPermissionsTest, NoApps) {
  ASSERT_FALSE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, OneAppDisabled) {
  SetAppUsageStats("app1", false, scope_);
  SetAppUsageStats("app2", false, scope_);
  ASSERT_FALSE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, OneAppEnabled) {
  SetAppUsageStats("app1", true, scope_);
  SetAppUsageStats("app2", false, scope_);
  ASSERT_TRUE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, UserInstallIgnoresSystem) {
  if (IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to system-scoped installs";
  }
  SetAppUsageStats("app1", false, UpdaterScope::kUser);
  SetAppUsageStats("app1", true, UpdaterScope::kSystem);
  ASSERT_FALSE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, SystemInstallLooksAtUser) {
  if (!IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to user-scoped installs";
  }
  SetAppUsageStats("app1", true, UpdaterScope::kUser);
  SetAppUsageStats("app1", false, UpdaterScope::kSystem);
  ASSERT_TRUE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, PermissionProviderAllowsRemoteLogging) {
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  ASSERT_TRUE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       PermissionProviderAllowsRemoteLoggingWithExemptApps) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  ASSERT_TRUE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest, UsageStatsProviderChecksPermissionProvider) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, false, scope_);
  ASSERT_FALSE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       PermissionProviderDisallowsRemoteLoggingWithOtherAppDisabled) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  SetAppUsageStats("unsupported_app", false, scope_);
  ASSERT_FALSE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       PermissionProviderDisallowsRemoteLoggingWithOtherAppEnabled) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  SetAppUsageStats("unsupported_app", true, scope_);
  ASSERT_FALSE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       SystemPermissionProviderAllowsRemoteLoggingWithUserAppEnabled) {
  if (!IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to user-scoped installs";
  }
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, UpdaterScope::kUser);
  SetAppUsageStats(fake_permission_provider_.app_id, false,
                   UpdaterScope::kSystem);
  ASSERT_TRUE(RemoteEventLoggingAllowed());
}

#endif

}  // namespace updater
