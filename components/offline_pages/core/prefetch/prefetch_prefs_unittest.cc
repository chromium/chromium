// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "base/test/scoped_feature_list.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class PrefetchPrefsTest : public testing::Test {
 public:
  void SetUp() override;

  TestingPrefServiceSimple* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

void PrefetchPrefsTest::SetUp() {
  prefetch_prefs::RegisterPrefs(prefs()->registry());
}

TEST_F(PrefetchPrefsTest, PrefetchingEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);
  EXPECT_FALSE(prefetch_prefs::IsEnabled(prefs()));
  prefetch_prefs::SetEnabledByServer(prefs(), true);
  EXPECT_TRUE(prefetch_prefs::IsEnabled(prefs()));

  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), false);
  EXPECT_FALSE(prefetch_prefs::IsEnabled(prefs()));

  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);
  // If disabled by default, should remain disabled.
  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), true);
  EXPECT_FALSE(prefetch_prefs::IsEnabled(prefs()));
}

TEST_F(PrefetchPrefsTest, LimitlessPrefetchingEnabled) {
  // Check that the default value is false.
  EXPECT_FALSE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Check that limitless can be enabled.
  prefetch_prefs::SetLimitlessPrefetchingEnabled(prefs(), true);
  EXPECT_TRUE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Check that it can be disabled.
  prefetch_prefs::SetLimitlessPrefetchingEnabled(prefs(), false);
  EXPECT_FALSE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Simulate time passing to check that the setting turns itself off as
  // expected.
  base::TimeDelta enabled_duration;
  if (version_info::IsOfficialBuild())
    enabled_duration = base::TimeDelta::FromDays(1);
  else
    enabled_duration = base::TimeDelta::FromDays(365);

  base::TimeDelta advance_delta = base::TimeDelta::FromHours(2);
  base::Time now = OfflineTimeNow();

  prefetch_prefs::SetLimitlessPrefetchingEnabled(prefs(), true);
  TestScopedOfflineClock test_clock;

  // Set time to just before the setting expires:
  test_clock.SetNow(now + enabled_duration - advance_delta);
  EXPECT_TRUE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Advance to just after it expires:
  test_clock.Advance(2 * advance_delta);
  EXPECT_FALSE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));
}

TEST_F(PrefetchPrefsTest, TestingHeaderValuePref) {
  // Should be empty string by default.
  EXPECT_EQ(std::string(), prefetch_prefs::GetPrefetchTestingHeader(prefs()));

  prefetch_prefs::SetPrefetchTestingHeader(prefs(), "ForceEnable");
  EXPECT_EQ("ForceEnable", prefetch_prefs::GetPrefetchTestingHeader(prefs()));

  prefetch_prefs::SetPrefetchTestingHeader(prefs(), "ForceDisable");
  EXPECT_EQ("ForceDisable", prefetch_prefs::GetPrefetchTestingHeader(prefs()));

  // We're not doing any checking/changing of the value (the server does that).
  prefetch_prefs::SetPrefetchTestingHeader(prefs(), "asdfasdfasdf");
  EXPECT_EQ("asdfasdfasdf", prefetch_prefs::GetPrefetchTestingHeader(prefs()));
}

TEST_F(PrefetchPrefsTest, EnabledByServer) {
  EXPECT_FALSE(prefetch_prefs::IsEnabledByServer(prefs()));

  prefetch_prefs::SetEnabledByServer(prefs(), true);
  EXPECT_TRUE(prefetch_prefs::IsEnabledByServer(prefs()));

  prefetch_prefs::SetEnabledByServer(prefs(), false);
  EXPECT_FALSE(prefetch_prefs::IsEnabledByServer(prefs()));
}

TEST_F(PrefetchPrefsTest, ForbiddenCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);

  // Check should be due in seven days.
  prefetch_prefs::SetEnabledByServer(prefs(), false);
  EXPECT_FALSE(prefetch_prefs::IsForbiddenCheckDue(prefs()));

  TestScopedOfflineClock test_clock;
  base::Time later = OfflineTimeNow() + base::TimeDelta::FromDays(8);
  test_clock.SetNow(later);

  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), false);
  EXPECT_FALSE(prefetch_prefs::IsForbiddenCheckDue(prefs()));
  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), true);
  EXPECT_TRUE(prefetch_prefs::IsForbiddenCheckDue(prefs()));

  // The check is not due if we are server-enabled.
  prefetch_prefs::SetEnabledByServer(prefs(), true);
  EXPECT_FALSE(prefetch_prefs::IsForbiddenCheckDue(prefs()));

  // Simulate the feature being disabled.
  test_clock.SetNow(OfflineTimeNow());
  prefetch_prefs::SetEnabledByServer(prefs(), false);

  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);
  test_clock.SetNow(later);
  EXPECT_FALSE(prefetch_prefs::IsForbiddenCheckDue(prefs()));
}

TEST_F(PrefetchPrefsTest, FirstForbiddenCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);

  EXPECT_TRUE(prefetch_prefs::IsForbiddenCheckDue(prefs()));
  EXPECT_TRUE(prefetch_prefs::IsEnabledByServerUnknown(prefs()));

  // Pretend a check was performed and failed.
  prefetch_prefs::SetEnabledByServer(prefs(), false);

  // Jump ahead in time so that a check should be due.
  TestScopedOfflineClock test_clock;
  test_clock.SetNow(OfflineTimeNow() + base::TimeDelta::FromDays(8));

  EXPECT_TRUE(prefetch_prefs::IsForbiddenCheckDue(prefs()));
  EXPECT_FALSE(prefetch_prefs::IsEnabledByServerUnknown(prefs()));
}

}  // namespace offline_pages
