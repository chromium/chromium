// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/live_caption_in_product_help.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "media/base/media_switches.h"

using ::testing::_;
using ::testing::Return;

using MockTracker = ::testing::NiceMock<feature_engagement::test::MockTracker>;

class LiveCaptionInProductHelpTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {media::kLiveCaption, feature_engagement::kIPHLiveCaptionFeature}, {});
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{feature_engagement::TrackerFactory::GetInstance(),
             base::BindRepeating(CreateTracker)}};
  }

  MockTracker* GetMockTracker() {
    return static_cast<MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(profile()));
  }

 private:
  static std::unique_ptr<KeyedService> CreateTracker(
      content::BrowserContext* context) {
    return std::make_unique<MockTracker>();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LiveCaptionInProductHelpTest, TriggersAndDismissesIPH) {
  LiveCaptionInProductHelp live_caption_iph(profile());

  auto* mock_tracker = GetMockTracker();
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker, Dismissed(_)).Times(1);

  BrowserList::SetLastActive(browser());
  live_caption_iph.OnMediaButtonEnabled();
  live_caption_iph.HelpDismissed();
}
