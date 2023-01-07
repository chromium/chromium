// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/win/group_policy_manager.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

#define TEST_APP_ID L"{D07D2B56-F583-4631-9E8E-9942F63765BE}"

class GroupPolicyManagerTests : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  registry_util::RegistryOverrideManager registry_override_;

 private:
  void DeletePolicyKey();
};

void GroupPolicyManagerTests::SetUp() {
  DeletePolicyKey();
}

void GroupPolicyManagerTests::TearDown() {
  DeletePolicyKey();
}

void GroupPolicyManagerTests::DeletePolicyKey() {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  base::win::RegKey key(HKEY_LOCAL_MACHINE, L"", Wow6432(DELETE));
  LONG result = key.DeleteKey(UPDATER_POLICIES_KEY);
  ASSERT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
}

TEST_F(GroupPolicyManagerTests, NoPolicySet) {
  std::unique_ptr<PolicyManagerInterface> policy_manager =
      std::make_unique<GroupPolicyManager>();
  EXPECT_FALSE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->source(), "GroupPolicy");

  int check_period = 0;
  EXPECT_FALSE(policy_manager->GetLastCheckPeriodMinutes(&check_period));

  UpdatesSuppressedTimes suppressed_times;
  EXPECT_FALSE(policy_manager->GetUpdatesSuppressedTimes(&suppressed_times));

  std::string download_preference;
  EXPECT_FALSE(
      policy_manager->GetDownloadPreferenceGroupPolicy(&download_preference));

  int cache_size_limit = 0;
  EXPECT_FALSE(
      policy_manager->GetPackageCacheSizeLimitMBytes(&cache_size_limit));
  int cache_life_limit = 0;
  EXPECT_FALSE(
      policy_manager->GetPackageCacheExpirationTimeDays(&cache_life_limit));

  std::string proxy_mode;
  EXPECT_FALSE(policy_manager->GetProxyMode(&proxy_mode));
  std::string proxy_server;
  EXPECT_FALSE(policy_manager->GetProxyServer(&proxy_server));
  std::string proxy_pac_url;
  EXPECT_FALSE(policy_manager->GetProxyPacUrl(&proxy_pac_url));

  std::string app_id = base::WideToUTF8(TEST_APP_ID);
  int install_policy = -1;
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      app_id, &install_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global", &install_policy));

  int update_policy = -1;
  EXPECT_FALSE(
      policy_manager->GetEffectivePolicyForAppUpdates(app_id, &update_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global", &update_policy));

  std::string target_channel;
  EXPECT_FALSE(policy_manager->GetTargetChannel(app_id, &target_channel));
  EXPECT_FALSE(
      policy_manager->GetTargetChannel("non-exist-app", &target_channel));

  std::string target_version_prefix;
  EXPECT_FALSE(
      policy_manager->GetTargetVersionPrefix(app_id, &target_version_prefix));
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix("non-exist-app",
                                                      &target_version_prefix));

  bool is_rollback_allowed = false;
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      app_id, &is_rollback_allowed));
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      "non-exist-app", &is_rollback_allowed));
}

TEST_F(GroupPolicyManagerTests, PolicyRead) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  base::win::RegKey key(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                        Wow6432(KEY_ALL_ACCESS));

  // Set global policies.
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"AutoUpdateCheckPeriodMinutes", 480));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"UpdatesSuppressedStartHour", 2));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"UpdatesSuppressedStartMin", 30));
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"UpdatesSuppressedDurationMin", 500));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"DownloadPreference", L"cacheable"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"PackageCacheSizeLimit", 100));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"PackageCacheLifeLimit", 45));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProxyMode", L"fixed_servers"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProxyServer", L"http://foo.bar"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProxyPacUrl", L"go/pac.url"));

  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"InstallDefault", 2));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"UpdateDefault", 1));

  // Set app policies
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"Install" TEST_APP_ID, 3));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"Update" TEST_APP_ID, 2));
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"TargetVersionPrefix" TEST_APP_ID, L"55.55."));
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"TargetChannel" TEST_APP_ID, L"beta"));
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"RollbackToTargetVersion" TEST_APP_ID, 1));

  std::unique_ptr<PolicyManagerInterface> policy_manager =
      std::make_unique<GroupPolicyManager>();
  EXPECT_EQ(policy_manager->HasActiveDevicePolicies(),
            base::win::IsEnrolledToDomain());

  int check_period = 0;
  EXPECT_TRUE(policy_manager->GetLastCheckPeriodMinutes(&check_period));
  EXPECT_EQ(check_period, 480);

  UpdatesSuppressedTimes suppressed_times = {};
  EXPECT_TRUE(policy_manager->GetUpdatesSuppressedTimes(&suppressed_times));
  EXPECT_EQ(suppressed_times.start_hour_, 2);
  EXPECT_EQ(suppressed_times.start_minute_, 30);
  EXPECT_EQ(suppressed_times.duration_minute_, 500);

  std::string download_preference;
  EXPECT_TRUE(
      policy_manager->GetDownloadPreferenceGroupPolicy(&download_preference));
  EXPECT_EQ(download_preference, "cacheable");

  int cache_size_limit = 0;
  EXPECT_TRUE(
      policy_manager->GetPackageCacheSizeLimitMBytes(&cache_size_limit));
  EXPECT_EQ(cache_size_limit, 100);
  int cache_life_limit = 0;
  EXPECT_TRUE(
      policy_manager->GetPackageCacheExpirationTimeDays(&cache_life_limit));
  EXPECT_EQ(cache_life_limit, 45);

  std::string proxy_mode;
  EXPECT_TRUE(policy_manager->GetProxyMode(&proxy_mode));
  EXPECT_EQ(proxy_mode, "fixed_servers");
  std::string proxy_server;
  EXPECT_TRUE(policy_manager->GetProxyServer(&proxy_server));
  EXPECT_EQ(proxy_server, "http://foo.bar");
  std::string proxy_pac_url;
  EXPECT_TRUE(policy_manager->GetProxyPacUrl(&proxy_pac_url));
  EXPECT_EQ(proxy_pac_url, "go/pac.url");

  std::string app_id = base::WideToUTF8(TEST_APP_ID);
  int install_policy = -1;
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppInstalls(
      app_id, &install_policy));
  EXPECT_EQ(install_policy, 3);
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global", &install_policy));
  EXPECT_EQ(install_policy, 2);

  int update_policy = -1;
  EXPECT_TRUE(
      policy_manager->GetEffectivePolicyForAppUpdates(app_id, &update_policy));
  EXPECT_EQ(update_policy, 2);
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global", &update_policy));
  EXPECT_EQ(update_policy, 1);

  std::string target_channel;
  EXPECT_TRUE(policy_manager->GetTargetChannel(app_id, &target_channel));
  EXPECT_EQ(target_channel, "beta");
  EXPECT_FALSE(
      policy_manager->GetTargetChannel("non-exist-app", &target_channel));

  std::string target_version_prefix;
  EXPECT_TRUE(
      policy_manager->GetTargetVersionPrefix(app_id, &target_version_prefix));
  EXPECT_EQ(target_version_prefix, "55.55.");
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix("non-exist-app",
                                                      &target_version_prefix));

  bool is_rollback_allowed = false;
  EXPECT_TRUE(policy_manager->IsRollbackToTargetVersionAllowed(
      app_id, &is_rollback_allowed));
  EXPECT_TRUE(is_rollback_allowed);
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      "non-exist-app", &is_rollback_allowed));
}

TEST_F(GroupPolicyManagerTests, WrongPolicyValueType) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  base::win::RegKey key(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                        Wow6432(KEY_ALL_ACCESS));

  // Set global policies.
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"AutoUpdateCheckPeriodMinutes", L"NotAnInteger"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"UpdatesSuppressedStartHour", L""));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"UpdatesSuppressedStartMin", L"30"));
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"UpdatesSuppressedDurationMin", L"WrongType"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"DownloadPreference", 15));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"PackageCacheSizeLimit", L"100"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"PackageCacheLifeLimit", L"45"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProxyMode", 10));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProxyServer", 1));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProxyPacUrl", 2));

  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"InstallDefault", L"install"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"UpdateDefault", L"automatic"));

  // Set app policies
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"Install" TEST_APP_ID, L"3"));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"Update" TEST_APP_ID, L"2"));
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"TargetVersionPrefix" TEST_APP_ID, 55));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"TargetChannel" TEST_APP_ID, 10));
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"RollbackToTargetVersion" TEST_APP_ID, L"1"));

  std::unique_ptr<PolicyManagerInterface> policy_manager =
      std::make_unique<GroupPolicyManager>();

  int check_period = 0;
  EXPECT_FALSE(policy_manager->GetLastCheckPeriodMinutes(&check_period));

  UpdatesSuppressedTimes suppressed_times = {};
  EXPECT_FALSE(policy_manager->GetUpdatesSuppressedTimes(&suppressed_times));

  std::string download_preference;
  EXPECT_FALSE(
      policy_manager->GetDownloadPreferenceGroupPolicy(&download_preference));

  int cache_size_limit = 0;
  EXPECT_FALSE(
      policy_manager->GetPackageCacheSizeLimitMBytes(&cache_size_limit));
  int cache_life_limit = 0;
  EXPECT_FALSE(
      policy_manager->GetPackageCacheExpirationTimeDays(&cache_life_limit));

  std::string proxy_mode;
  EXPECT_FALSE(policy_manager->GetProxyMode(&proxy_mode));
  std::string proxy_server;
  EXPECT_FALSE(policy_manager->GetProxyServer(&proxy_server));
  std::string proxy_pac_url;
  EXPECT_FALSE(policy_manager->GetProxyPacUrl(&proxy_pac_url));

  std::string app_id = base::WideToUTF8(TEST_APP_ID);
  int install_policy = -1;
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      app_id, &install_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global", &install_policy));

  int update_policy = -1;
  EXPECT_FALSE(
      policy_manager->GetEffectivePolicyForAppUpdates(app_id, &update_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global", &update_policy));

  std::string target_channel;
  EXPECT_FALSE(policy_manager->GetTargetChannel(app_id, &target_channel));
  EXPECT_FALSE(
      policy_manager->GetTargetChannel("non-exist-app", &target_channel));

  std::string target_version_prefix;
  EXPECT_FALSE(
      policy_manager->GetTargetVersionPrefix(app_id, &target_version_prefix));
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix("non-exist-app",
                                                      &target_version_prefix));

  bool is_rollback_allowed = false;
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      app_id, &is_rollback_allowed));
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      "non-exist-app", &is_rollback_allowed));
}

}  // namespace updater
