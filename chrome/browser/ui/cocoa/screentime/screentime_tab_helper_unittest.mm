// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/cocoa/screentime/fake_webpage_controller.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"
#include "chrome/browser/ui/cocoa/screentime/tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screentime {

class ScreentimeTabHelperTest : public ::testing::Test {
 public:
  ScreentimeTabHelperTest() = default;
  ~ScreentimeTabHelperTest() override = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    TabHelper::UseFakeWebpageControllerForTesting();
    // `kMediaRouter` is disabled because it has unmet dependencies and is
    // unrelated to this unit test.
    features_.InitWithFeatures(/*enabled=*/{kScreenTime},
                               /*disabled=*/{media_router::kMediaRouter});
    profile_ = std::make_unique<TestingProfile>();
  }

  TestingProfile* profile() const { return profile_.get(); }

 private:
  base::test::ScopedFeatureList features_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ScreentimeTabHelperTest, NeverUsedInIncognito) {
  if (@available(macOS 12.1, *)) {
    auto* otr_profile = profile()->GetOffTheRecordProfile(
        Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);

    EXPECT_TRUE(TabHelper::IsScreentimeEnabledForProfile(profile()));
    EXPECT_FALSE(TabHelper::IsScreentimeEnabledForProfile(otr_profile));
  } else {
    GTEST_SKIP() << "ScreenTime is only enabled on macOS 12.1 and higher";
  }
}

TEST_F(ScreentimeTabHelperTest, OnlyOriginsAreReported) {
  auto contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto helper = std::make_unique<TabHelper>(contents.get());

  FakeWebpageController* controller = static_cast<FakeWebpageController*>(
      helper->page_controller_for_testing());

  auto* tester = content::WebContentsTester::For(contents.get());
  tester->NavigateAndCommit(GURL("https://www.chromium.org/abc"));
  tester->NavigateAndCommit(GURL("https://test.chromium.org/def"));

  EXPECT_EQ(controller->visited_urls_for_testing()[0],
            GURL("https://www.chromium.org/"));
  EXPECT_EQ(controller->visited_urls_for_testing()[1],
            GURL("https://test.chromium.org/"));
}

TEST_F(ScreentimeTabHelperTest, OnlyHttpHttpsSchemesReported) {
  auto contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto helper = std::make_unique<TabHelper>(contents.get());

  FakeWebpageController* controller = static_cast<FakeWebpageController*>(
      helper->page_controller_for_testing());

  auto* tester = content::WebContentsTester::For(contents.get());
  tester->NavigateAndCommit(GURL("https://www.chromium.org/abc"));
  tester->NavigateAndCommit(GURL("http://test.chromium.org/def"));
  tester->NavigateAndCommit(GURL("chrome://version"));
  tester->NavigateAndCommit(GURL("mailto:hello@example.com"));

  EXPECT_EQ(controller->visited_urls_for_testing().size(), 2u);

  EXPECT_EQ(controller->visited_urls_for_testing()[0],
            GURL("https://www.chromium.org/"));
  EXPECT_EQ(controller->visited_urls_for_testing()[1],
            GURL("http://test.chromium.org/"));
}

TEST_F(ScreentimeTabHelperTest, EnterprisePolicy) {
  if (@available(macOS 12.1, *)) {
    profile()->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kScreenTimeEnabled,
        std::make_unique<base::Value>(false));
    EXPECT_FALSE(TabHelper::IsScreentimeEnabledForProfile(profile()));
    profile()->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kScreenTimeEnabled,
        std::make_unique<base::Value>(true));
    EXPECT_TRUE(TabHelper::IsScreentimeEnabledForProfile(profile()));
  } else {
    GTEST_SKIP() << "ScreenTime is only enabled on macOS 12.1 and higher";
  }
}

}  // namespace screentime
