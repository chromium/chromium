// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

constexpr char kTestAppID[] = "{d07d2b56-f583-4631-9e8e-9942f63765be}";
constexpr char kTestAppIDForceInstall[] = "appidforceinstall";

class PolicyManagerTests : public ::testing::Test {};

TEST_F(PolicyManagerTests, NoPolicySet) {
  auto policy_manager =
      base::MakeRefCounted<PolicyManager>(base::Value::Dict());
  EXPECT_FALSE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->source(), "DictValuePolicy");

  EXPECT_EQ(policy_manager->CloudPolicyOverridesPlatformPolicy(), std::nullopt);
  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), std::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreference(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(kTestAppID),
            std::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID),
            std::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetTargetChannel(kTestAppID), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kTestAppID), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kTestAppID),
            std::nullopt);
  EXPECT_FALSE(
      policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"));
  EXPECT_EQ(policy_manager->GetForceInstallApps(), std::nullopt);
}

TEST_F(PolicyManagerTests, PolicyRead) {
  base::Value::Dict policies;

  policies.Set("autoupdatecheckperiodminutes", 480);
  policies.Set("updatessuppressedstarthour", 2);
  policies.Set("updatessuppressedstartmin", 30);
  policies.Set("updatessuppresseddurationmin", 500);
  policies.Set("downloadpreference", "cacheable");
  policies.Set("packagecachesizelimit", 100);
  policies.Set("packagecachelifelimit", 45);
  policies.Set("proxymode", "fixed_servers");
  policies.Set("proxyserver", "http://foo.bar");
  policies.Set("proxypacurl", "go/pac.url");

  policies.Set("installdefault", 2);
  policies.Set("updatedefault", 1);

  // Set app policies
  policies.Set(base::StrCat({"install", kTestAppID}), 3);
  policies.Set(base::StrCat({"update", kTestAppID}), 2);
  policies.Set(base::StrCat({"targetversionprefix", kTestAppID}), "55.55.");
  policies.Set(base::StrCat({"targetchannel", kTestAppID}), "beta");
  policies.Set(base::StrCat({"rollbacktotargetversion", kTestAppID}), 1);
  policies.Set(base::StrCat({"install", kTestAppIDForceInstall}),
               kPolicyForceInstallUser);

  auto policy_manager =
      base::MakeRefCounted<PolicyManager>(std::move(policies));

  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->CloudPolicyOverridesPlatformPolicy(), std::nullopt);
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

  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(kTestAppID), 3);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(
                "non-exist-app-fallback-to-global"),
            2);

  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID), 2);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(
                "non-exist-app-fallback-to-global"),
            1);

  EXPECT_EQ(policy_manager->GetTargetChannel(kTestAppID), "beta");
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), std::nullopt);

  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kTestAppID), "55.55.");
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            std::nullopt);

  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kTestAppID), true);
  EXPECT_FALSE(
      policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"));

  std::optional<std::vector<std::string>> force_install_apps =
      policy_manager->GetForceInstallApps();
  ASSERT_EQ(force_install_apps.has_value(), !IsSystemInstall());

  if (!IsSystemInstall()) {
    ASSERT_EQ(force_install_apps->size(), 1U);
    EXPECT_EQ(force_install_apps->at(0), kTestAppIDForceInstall);
  }
}

TEST_F(PolicyManagerTests, WrongPolicyValueType) {
  base::Value::Dict policies;

  // Set global policies.
  policies.Set("autoupdatecheckperiodminutes", "NotAnInteger");
  policies.Set("updatessuppressedstarthour", "");
  policies.Set("updatessuppressedstartmin", "30");
  policies.Set("updatessuppresseddurationmin", "WrongType");
  policies.Set("downloadpreference", 15);
  policies.Set("packagecachesizelimit", "100");
  policies.Set("packagecachelifelimit", "45");
  policies.Set("proxymode", 10);
  policies.Set("proxyserver", 1);
  policies.Set("proxypacurl", 2);

  policies.Set("installdefault", "install");
  policies.Set("updatedefault", "automatic");

  // Set app policies
  policies.Set(base::StrCat({"install", kTestAppID}), "3");
  policies.Set(base::StrCat({"update", kTestAppID}), "2");
  policies.Set(base::StrCat({"targetversionprefix", kTestAppID}), 55);
  policies.Set(base::StrCat({"targetchannel", kTestAppID}), 10);
  policies.Set(base::StrCat({"rollbacktotargetversion", kTestAppID}), "1");

  auto policy_manager =
      base::MakeRefCounted<PolicyManager>(std::move(policies));

  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->CloudPolicyOverridesPlatformPolicy(), std::nullopt);
  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), std::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreference(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(kTestAppID),
            std::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID),
            std::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetTargetChannel(kTestAppID), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kTestAppID), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kTestAppID),
            std::nullopt);
  EXPECT_FALSE(
      policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"));
}

}  // namespace updater
