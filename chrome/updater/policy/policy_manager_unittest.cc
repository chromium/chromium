// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

constexpr char kTestAppID[] = "{D07D2B56-F583-4631-9E8E-9942F63765BE}";
constexpr char kTestAppIDForceInstall[] = "AppIDForceInstall";

class PolicyManagerTests : public ::testing::Test {};

TEST_F(PolicyManagerTests, NoPolicySet) {
  auto policy_manager =
      base::MakeRefCounted<PolicyManager>(base::Value::Dict());
  EXPECT_FALSE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->source(), "DictValuePolicy");

  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreferenceGroupPolicy(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(kTestAppID),
            absl::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID),
            absl::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetTargetChannel(kTestAppID), absl::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), absl::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kTestAppID), absl::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            absl::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kTestAppID),
            absl::nullopt);
  EXPECT_FALSE(
      policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"));
  EXPECT_EQ(policy_manager->GetForceInstallApps(), absl::nullopt);
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
  policies.Set(base::StrCat({"Install", kTestAppIDForceInstall}),
               kPolicyForceInstallUser);

  auto policy_manager =
      base::MakeRefCounted<PolicyManager>(std::move(policies));

  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), base::Minutes(480));

  absl::optional<UpdatesSuppressedTimes> suppressed_times =
      policy_manager->GetUpdatesSuppressedTimes();
  ASSERT_TRUE(suppressed_times);
  EXPECT_EQ(suppressed_times->start_hour_, 2);
  EXPECT_EQ(suppressed_times->start_minute_, 30);
  EXPECT_EQ(suppressed_times->duration_minute_, 500);

  EXPECT_EQ(policy_manager->GetDownloadPreferenceGroupPolicy(), "cacheable");

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
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), absl::nullopt);

  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kTestAppID), "55.55.");
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            absl::nullopt);

  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kTestAppID), true);
  EXPECT_FALSE(
      policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"));

  absl::optional<std::vector<std::string>> force_install_apps =
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

  auto policy_manager =
      base::MakeRefCounted<PolicyManager>(std::move(policies));

  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());

  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreferenceGroupPolicy(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), absl::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(kTestAppID),
            absl::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppInstalls(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(kTestAppID),
            absl::nullopt);
  EXPECT_FALSE(policy_manager->GetEffectivePolicyForAppUpdates(
      "non-exist-app-fallback-to-global"));
  EXPECT_EQ(policy_manager->GetTargetChannel(kTestAppID), absl::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel("non-exist-app"), absl::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kTestAppID), absl::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix("non-exist-app"),
            absl::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kTestAppID),
            absl::nullopt);
  EXPECT_FALSE(
      policy_manager->IsRollbackToTargetVersionAllowed("non-exist-app"));
}

}  // namespace updater
