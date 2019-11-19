// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;

using MockTracker = ::testing::NiceMock<feature_engagement::test::MockTracker>;

class GlobalMediaControlsInProductHelpTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        feature_engagement::kIPHGlobalMediaControlsFeature);
  }

  // We want to use |MockTracker| instead of |Tracker|, so we must override its
  // factory.
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{feature_engagement::TrackerFactory::GetInstance(),
             base::BindRepeating(CreateTracker)}};
  }

  MockTracker* GetMockTracker() { return GetMockTrackerForProfile(profile()); }

  MockTracker* GetMockTrackerForProfile(Profile* profile) {
    return static_cast<MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(profile));
  }

 private:
  // Factory function for our |MockTracker|
  static std::unique_ptr<KeyedService> CreateTracker(
      content::BrowserContext* context) {
    return std::make_unique<MockTracker>();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlobalMediaControlsInProductHelpTest,
       OpenNewTabWhileMediaPlayingTriggersIPH) {
  GlobalMediaControlsInProductHelp gmc_iph(profile());

  auto* mock_tracker = GetMockTracker();
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
      .Times(1)
      .WillOnce(Return(true));

  // Start playing media in a tab.
  BrowserList::SetLastActive(browser());
  AddTab(browser(), GURL("chrome://blank"));
  gmc_iph.OnMediaButtonEnabled();

  // Open a new foreground tab.
  AddTab(browser(), GURL("chrome://blank"));
}

TEST_F(GlobalMediaControlsInProductHelpTest,
       OpenNewTabWhileMediaNotPlayingDoesntTriggerIPH) {
  GlobalMediaControlsInProductHelp gmc_iph(profile());

  auto* mock_tracker = GetMockTracker();
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_)).Times(0);

  // Have a tab with no media.
  BrowserList::SetLastActive(browser());
  AddTab(browser(), GURL("chrome://blank"));

  // Open a new foreground tab.
  AddTab(browser(), GURL("chrome://blank"));
}

TEST_F(GlobalMediaControlsInProductHelpTest, DoesNotTriggerForOtherProfiles) {
  std::unique_ptr<BrowserWindow> alt_window = CreateBrowserWindow();
  Profile* alt_profile = profile()->GetOffTheRecordProfile();
  std::unique_ptr<Browser> alt_browser =
      CreateBrowser(alt_profile, Browser::TYPE_NORMAL, false, alt_window.get());

  GlobalMediaControlsInProductHelp gmc_iph(profile());
  GlobalMediaControlsInProductHelp alt_gmc_iph(alt_profile);

  // The original profile should not trigger the IPH.
  auto* mock_tracker = GetMockTracker();
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_)).Times(0);

  // The alt one should.
  auto* alt_mock_tracker = GetMockTrackerForProfile(alt_profile);
  EXPECT_CALL(*alt_mock_tracker, ShouldTriggerHelpUI(_))
      .Times(1)
      .WillOnce(Return(true));

  // Start playing media.
  BrowserList::SetLastActive(browser());
  AddTab(browser(), GURL("chrome://blank"));
  gmc_iph.OnMediaButtonEnabled();

  // Switch to the other profile and play media.
  BrowserList::SetLastActive(alt_browser.get());
  AddTab(alt_browser.get(), GURL("chrome://blank"));
  alt_gmc_iph.OnMediaButtonEnabled();

  // Open a new foreground tab to the other profile.
  AddTab(alt_browser.get(), GURL("chrome://blank"));

  // Need to manually close all tabs before |alt_browser| is destroyed or we'll
  // crash.
  alt_browser->tab_strip_model()->CloseAllTabs();
}

TEST_F(GlobalMediaControlsInProductHelpTest, MediaStoppedDoesNotTriggerIPH) {
  GlobalMediaControlsInProductHelp gmc_iph(profile());

  auto* mock_tracker = GetMockTracker();
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_)).Times(0);

  // Start playing media in a tab.
  BrowserList::SetLastActive(browser());
  AddTab(browser(), GURL("chrome://blank"));
  gmc_iph.OnMediaButtonEnabled();

  // Stop playing media.
  gmc_iph.OnMediaButtonDisabled();

  // Open a new foreground tab.
  AddTab(browser(), GURL("chrome://blank"));
}
