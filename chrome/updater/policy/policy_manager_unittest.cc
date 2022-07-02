// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

constexpr char kTestAppID[] = "{D07D2B56-F583-4631-9E8E-9942F63765BE}";

class PolicyManagerTests : public ::testing::Test {};

TEST_F(PolicyManagerTests, NoPolicySet) {
  auto policy_manager = std::make_unique<PolicyManager>(base::Value::Dict());
  EXPECT_FALSE(policy_manager->IsManaged());

  EXPECT_EQ(policy_manager->source(), "DictValuePolicy");

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

  int install_policy = -1;
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      kTestAppID, &install_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global", &install_policy));

  int update_policy = -1;
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID,
                                                               &update_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global", &update_policy));

  std::string target_channel;
  EXPECT_FALSE(policy_manager->GetTargetChannel(kTestAppID, &target_channel));
  EXPECT_FALSE(
      policy_manager->GetTargetChannel("non-exist-app", &target_channel));

  std::string target_version_prefix;
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix(kTestAppID,
                                                      &target_version_prefix));
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix("non-exist-app",
                                                      &target_version_prefix));

  bool is_rollback_allowed = false;
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      kTestAppID, &is_rollback_allowed));
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      "non-exist-app", &is_rollback_allowed));
}

TEST_F(PolicyManagerTests, PolicyRead) {
  base::Value::Dict policies;

  policies.Set("AutoUpdateCheckPeriodMinutes", 480);
  policies.Set("UpdatesSuppressedStartHour", 2);
  policies.Set("UpdatesSuppressedStartMin", 30);
  policies.Set("UpdatesSuppressedDurationMin", 500);
  policies.Set("DownloadPreference", "cacheable");
  policies.Set("PackageCacheSizeLimit", 100);
  policies.Set("PackageCacheLifeLimit", 45);
  policies.Set("ProxyMode", "fixed_servers");
  policies.Set("ProxyServer", "http://foo.bar");
  policies.Set("ProxyPacUrl", "go/pac.url");

  policies.Set("InstallDefault", 2);
  policies.Set("UpdateDefault", 1);

  // Set app policies
  policies.Set(base::StrCat({"Install", kTestAppID}), 3);
  policies.Set(base::StrCat({"Update", kTestAppID}), 2);
  policies.Set(base::StrCat({"TargetVersionPrefix", kTestAppID}), "55.55.");
  policies.Set(base::StrCat({"TargetChannel", kTestAppID}), "beta");
  policies.Set(base::StrCat({"RollbackToTargetVersion", kTestAppID}), 1);

  auto policy_manager = std::make_unique<PolicyManager>(std::move(policies));

  EXPECT_TRUE(policy_manager->IsManaged());

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

  int install_policy = -1;
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppInstalls(
      kTestAppID, &install_policy));
  EXPECT_EQ(install_policy, 3);
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global", &install_policy));
  EXPECT_EQ(install_policy, 2);

  int update_policy = -1;
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID,
                                                              &update_policy));
  EXPECT_EQ(update_policy, 2);
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global", &update_policy));
  EXPECT_EQ(update_policy, 1);

  std::string target_channel;
  EXPECT_TRUE(policy_manager->GetTargetChannel(kTestAppID, &target_channel));
  EXPECT_EQ(target_channel, "beta");
  EXPECT_FALSE(
      policy_manager->GetTargetChannel("non-exist-app", &target_channel));

  std::string target_version_prefix;
  EXPECT_TRUE(policy_manager->GetTargetVersionPrefix(kTestAppID,
                                                     &target_version_prefix));
  EXPECT_EQ(target_version_prefix, "55.55.");
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix("non-exist-app",
                                                      &target_version_prefix));

  bool is_rollback_allowed = false;
  EXPECT_TRUE(policy_manager->IsRollbackToTargetVersionAllowed(
      kTestAppID, &is_rollback_allowed));
  EXPECT_TRUE(is_rollback_allowed);
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      "non-exist-app", &is_rollback_allowed));
}

TEST_F(PolicyManagerTests, WrongPolicyValueType) {
  base::Value::Dict policies;

  // Set global policies.
  policies.Set("AutoUpdateCheckPeriodMinutes", "NotAnInteger");
  policies.Set("UpdatesSuppressedStartHour", "");
  policies.Set("UpdatesSuppressedStartMin", "30");
  policies.Set("UpdatesSuppressedDurationMin", "WrongType");
  policies.Set("DownloadPreference", 15);
  policies.Set("PackageCacheSizeLimit", "100");
  policies.Set("PackageCacheLifeLimit", "45");
  policies.Set("ProxyMode", 10);
  policies.Set("ProxyServer", 1);
  policies.Set("ProxyPacUrl", 2);

  policies.Set("InstallDefault", "install");
  policies.Set("UpdateDefault", "automatic");

  // Set app policies
  policies.Set(base::StrCat({"Install", kTestAppID}), "3");
  policies.Set(base::StrCat({"Update", kTestAppID}), "2");
  policies.Set(base::StrCat({"TargetVersionPrefix", kTestAppID}), 55);
  policies.Set(base::StrCat({"TargetChannel", kTestAppID}), 10);
  policies.Set(base::StrCat({"RollbackToTargetVersion", kTestAppID}), "1");

  auto policy_manager = std::make_unique<PolicyManager>(std::move(policies));

  EXPECT_TRUE(policy_manager->IsManaged());

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

  int install_policy = -1;
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      kTestAppID, &install_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global", &install_policy));

  int update_policy = -1;
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID,
                                                               &update_policy));
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global", &update_policy));

  std::string target_channel;
  EXPECT_FALSE(policy_manager->GetTargetChannel(kTestAppID, &target_channel));
  EXPECT_FALSE(
      policy_manager->GetTargetChannel("non-exist-app", &target_channel));

  std::string target_version_prefix;
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix(kTestAppID,
                                                      &target_version_prefix));
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix("non-exist-app",
                                                      &target_version_prefix));

  bool is_rollback_allowed = false;
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      kTestAppID, &is_rollback_allowed));
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      "non-exist-app", &is_rollback_allowed));
}

}  // namespace updater
