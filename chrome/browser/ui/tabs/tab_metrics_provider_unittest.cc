// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
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
