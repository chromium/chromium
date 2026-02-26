// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabMetricsProviderTest : public testing::Test {
 public:
  TabMetricsProviderTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~TabMetricsProviderTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
  }

  ProfileManager* profile_manager() {
    return testing_profile_manager_.profile_manager();
  }

  TestingProfile* CreateTestingProfile(const std::string& profile_name) {
    return testing_profile_manager_.CreateTestingProfile(profile_name);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(TabMetricsProviderTest, AllVerticalProfiles) {
  TestingProfile* profile = CreateTestingProfile("p1");
  profile->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, true);

  TabMetricsProvider provider(profile_manager());
  EXPECT_EQ(provider.GetVerticalTabsState(), VerticalTabsState::kAllVertical);
}

TEST_F(TabMetricsProviderTest, AllHorizontalProfiles) {
  TestingProfile* profile = CreateTestingProfile("p1");
  profile->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, false);

  TabMetricsProvider provider(profile_manager());
  EXPECT_EQ(provider.GetVerticalTabsState(), VerticalTabsState::kAllHorizontal);
}

TEST_F(TabMetricsProviderTest, MixedProfiles) {
  TestingProfile* p1 = CreateTestingProfile("p1");
  p1->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, true);
  TestingProfile* p2 = CreateTestingProfile("p2");
  p2->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, false);

  TabMetricsProvider provider(profile_manager());
  EXPECT_EQ(provider.GetVerticalTabsState(), VerticalTabsState::kMixed);
}

TEST_F(TabMetricsProviderTest, ProvideCurrentSessionData) {
  TestingProfile* p1 = CreateTestingProfile("p1");
  p1->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, true);
  TestingProfile* p2 = CreateTestingProfile("p2");
  p2->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, false);

  base::HistogramTester histogram_tester;
  TabMetricsProvider provider(profile_manager());
  provider.ProvideCurrentSessionData(nullptr);
  histogram_tester.ExpectUniqueSample("Tabs.VerticalTabs.State",
                                      VerticalTabsState::kMixed, 1);
}

TEST_F(TabMetricsProviderTest, NoProfiles) {
  base::HistogramTester histogram_tester;
  TabMetricsProvider provider(profile_manager());
  EXPECT_EQ(provider.GetVerticalTabsState(), VerticalTabsState::kAllHorizontal);

  provider.ProvideCurrentSessionData(nullptr);
  EXPECT_EQ(0, histogram_tester.GetTotalSum("Tabs.VerticalTabs.State"));
}

TEST_F(TabMetricsProviderTest, LogsSessionStartMetrics) {
  // Create two profiles, one with vertical tabs enabled, and the other
  // disabled.
  TestingProfile* p1 = CreateTestingProfile("p1");
  p1->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, true);
  TestingProfile* p2 = CreateTestingProfile("p2");
  p2->GetPrefs()->SetBoolean(prefs::kVerticalTabsEnabled, false);

  base::HistogramTester histogram_tester;
  ASSERT_EQ(0, histogram_tester.GetTotalSum(
                   "Tabs.VerticalTabs.EnabledAtSessionStart"));

  // On TabMetricsProvider creation, log session start metric for two existing
  // profiles.
  TabMetricsProvider provider(profile_manager());
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     false, 1);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     true, 1);

  auto notify_session_start = [&](Profile* profile) {
    UserEducationService* user_education_service =
        UserEducationServiceFactory::GetForBrowserContext(profile);
    std::unique_ptr<user_education::test::TestIdleObserver> test_idle_observer =
        std::make_unique<user_education::test::TestIdleObserver>(base::Time());
    user_education::test::TestIdleObserver* test_idle_observer_ptr =
        test_idle_observer.get();
    user_education_service->user_education_session_manager()
        .ReplaceIdleObserverForTesting(std::move(test_idle_observer));
    test_idle_observer_ptr->SetLastActiveTime(
        base::Time::Now() +
            user_education::features::GetIdleTimeBetweenSessions(),
        true);
  };

  // Send a notification that the first profile has started a new session.
  notify_session_start(p1);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     false, 1);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     true, 2);

  // Send a notification that the second profile has started a new session.
  notify_session_start(p2);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     false, 2);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     true, 2);

  auto create_new_profile = [&](std::string profile_name,
                                std::u16string user_name,
                                bool vertical_tabs_enabled) {
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    prefs->SetBoolean(prefs::kVerticalTabsEnabled, vertical_tabs_enabled);
    testing_profile_manager_.CreateTestingProfile(
        profile_name, std::move(prefs), user_name, /*avatar_id=*/0, {});
  };

  // Create a third profile with vertical tabs disabled.
  create_new_profile("p3", u"p3", false);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     false, 3);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     true, 2);

  // Create a fourth profile with vertical tabs disabled.
  create_new_profile("p4", u"p4", true);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     false, 3);
  histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.EnabledAtSessionStart",
                                     true, 3);
}
