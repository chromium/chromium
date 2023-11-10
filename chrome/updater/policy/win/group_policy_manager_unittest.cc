// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/win/group_policy_manager.h"

#include <optional>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
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

  base::test::TaskEnvironment environment_;
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
  auto policy_manager = base::MakeRefCounted<GroupPolicyManager>(true);
  EXPECT_FALSE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->source(), "Group Policy");
  EXPECT_FALSE(policy_manager->CloudPolicyOverridesPlatformPolicy());

  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), std::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreference(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), std::nullopt);

  std::string app_id = base::WideToUTF8(TEST_APP_ID);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(app_id),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(
                "non-exist-app-fallback-to-global"),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(app_id),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(
                "non-exist-app-fallback-to-global"),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel(app_id), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(app_id), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(app_id),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"),
            std::nullopt);
}

TEST_F(GroupPolicyManagerTests, PolicyRead) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  base::win::RegKey key(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                        Wow6432(KEY_ALL_ACCESS));

  // Set global policies.
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"CloudPolicyOverridesPlatformPolicy", 1));
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

  auto policy_manager = base::MakeRefCounted<GroupPolicyManager>(true);
  EXPECT_EQ(policy_manager->HasActiveDevicePolicies(),
            base::win::IsEnrolledToDomain());

  EXPECT_TRUE(policy_manager->CloudPolicyOverridesPlatformPolicy());
  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), base::Minutes(480));

  std::optional<UpdatesSuppressedTimes> suppressed_times =
      policy_manager->GetUpdatesSuppressedTimes();
  ASSERT_TRUE(suppressed_times);
  EXPECT_EQ(suppressed_times->start_hour_, 2);
  EXPECT_EQ(suppressed_times->start_minute_, 30);
  EXPECT_EQ(suppressed_times->duration_minute_, 500);

  EXPECT_EQ(policy_manager->GetDownloadPreference(), "cacheable");
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), 100);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), 45);
  EXPECT_EQ(policy_manager->GetProxyMode(), "fixed_servers");
  EXPECT_EQ(policy_manager->GetProxyServer(), "http://foo.bar");
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), "go/pac.url");

  std::string app_id = base::WideToUTF8(TEST_APP_ID);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(app_id), 3);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(
                "non-exist-app-fallback-to-global"),
            2);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(app_id), 2);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(
                "non-exist-app-fallback-to-global"),
            1);
  EXPECT_EQ(policy_manager->GetTargetChannel(app_id), "beta");
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(app_id), "55.55.");
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(app_id), true);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"),
            std::nullopt);
}

TEST_F(GroupPolicyManagerTests, WrongPolicyValueType) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  base::win::RegKey key(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                        Wow6432(KEY_ALL_ACCESS));

  // Set global policies.
  EXPECT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"CloudPolicyOverridesPlatformPolicy", L"1"));
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

  auto policy_manager = base::MakeRefCounted<GroupPolicyManager>(true, true);
  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());

  EXPECT_FALSE(policy_manager->CloudPolicyOverridesPlatformPolicy());
  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), std::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreference(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), std::nullopt);

  std::string app_id = base::WideToUTF8(TEST_APP_ID);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(app_id),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(
                "non-exist-app-fallback-to-global"),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(app_id),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(
                "non-exist-app-fallback-to-global"),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel(app_id), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(app_id), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(app_id),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"),
            std::nullopt);
}

}  // namespace updater
