// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider_metrics_provider.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "components/site_token_provider/site_token_provider_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace site_token_provider {
namespace {

class SiteTokenProviderMetricsProviderTest : public testing::Test {
 protected:
  // Adds a profile whose synced site token provider pref is set to `enabled`,
  // or left at its default (unset) value when `enabled` is `std::nullopt`.
  void AddProfile(std::optional<bool> enabled = std::nullopt) {
    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    prefs::RegisterProfilePrefs(pref_service->registry());
    if (enabled.has_value()) {
      pref_service->SetBoolean(prefs::kSiteTokenProviderPref, *enabled);
    }
    profile_prefs_.push_back(std::move(pref_service));
  }

  metrics::SystemProfileProto ProvideSystemProfile() {
    metrics::SystemProfileProto system_profile;
    provider_.ProvideSystemProfileMetrics(&system_profile);
    return system_profile;
  }

 private:
  std::vector<const PrefService*> GetProfilePrefs() const {
    std::vector<const PrefService*> result;
    for (const auto& pref_service : profile_prefs_) {
      result.push_back(pref_service.get());
    }
    return result;
  }

  std::vector<std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>>
      profile_prefs_;
  SiteTokenProviderMetricsProvider provider_{base::BindRepeating(
      &SiteTokenProviderMetricsProviderTest::GetProfilePrefs,
      base::Unretained(this))};
};

TEST_F(SiteTokenProviderMetricsProviderTest, NoProfilesLeavesStateUnset) {
  EXPECT_FALSE(ProvideSystemProfile().has_site_token_provider_state());
}

TEST_F(SiteTokenProviderMetricsProviderTest, SingleProfileDefaultUnsetPref) {
  AddProfile();

  const metrics::SystemProfileProto system_profile = ProvideSystemProfile();

  ASSERT_TRUE(
      system_profile.site_token_provider_state().has_sync_pref_enabled());
  EXPECT_FALSE(system_profile.site_token_provider_state().sync_pref_enabled());
}

TEST_F(SiteTokenProviderMetricsProviderTest, SingleProfileDisabled) {
  AddProfile(/*enabled=*/false);

  const metrics::SystemProfileProto system_profile = ProvideSystemProfile();

  ASSERT_TRUE(
      system_profile.site_token_provider_state().has_sync_pref_enabled());
  EXPECT_FALSE(system_profile.site_token_provider_state().sync_pref_enabled());
}

TEST_F(SiteTokenProviderMetricsProviderTest, SingleProfileEnabled) {
  AddProfile(/*enabled=*/true);

  EXPECT_TRUE(
      ProvideSystemProfile().site_token_provider_state().sync_pref_enabled());
}

TEST_F(SiteTokenProviderMetricsProviderTest, AnyProfileEnabledReportsTrue) {
  AddProfile(/*enabled=*/false);
  AddProfile(/*enabled=*/true);
  AddProfile(/*enabled=*/false);

  EXPECT_TRUE(
      ProvideSystemProfile().site_token_provider_state().sync_pref_enabled());
}

}  // namespace
}  // namespace site_token_provider
