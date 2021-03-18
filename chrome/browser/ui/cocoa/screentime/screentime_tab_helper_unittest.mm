// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/cocoa/screentime/fake_webpage_controller.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"
#include "chrome/browser/ui/cocoa/screentime/tab_helper.h"
#include "chrome/test/base/testing_profile.h"
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
    features_.InitAndEnableFeature(kScreenTime);
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
  auto* otr_profile =
      profile()->GetOffTheRecordProfile(Profile::OTRProfileID::PrimaryID());

  EXPECT_TRUE(TabHelper::IsScreentimeEnabledForProfile(profile()));
  EXPECT_FALSE(TabHelper::IsScreentimeEnabledForProfile(otr_profile));
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

}  // namespace screentime
