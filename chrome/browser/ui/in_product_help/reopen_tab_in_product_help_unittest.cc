// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_trigger.h"
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

namespace {

constexpr base::TimeDelta kTabMinimumActiveDuration =
    base::TimeDelta::FromSeconds(15);
constexpr base::TimeDelta kNewTabOpenedTimeout =
    base::TimeDelta::FromSeconds(5);

}  // namespace

class ReopenTabInProductHelpTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHReopenTabFeature,
        ReopenTabInProductHelpTrigger::GetFieldTrialParamsForTest(
            kTabMinimumActiveDuration.InSeconds(),
            kNewTabOpenedTimeout.InSeconds()));
  }
  // We want to use |MockTracker| instead of |Tracker|, so we must override its
  // factory.
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{feature_engagement::TrackerFactory::GetInstance(),
             base::BindRepeating(CreateTracker)}};
  }

  MockTracker* GetMockTracker() {
    return static_cast<MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(profile()));
  }

  base::SimpleTestTickClock* clock() { return &clock_; }

 private:
  // Factory function for our |MockTracker|
  static std::unique_ptr<KeyedService> CreateTracker(
      content::BrowserContext* context) {
    return std::make_unique<MockTracker>();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock clock_;
};

TEST_F(ReopenTabInProductHelpTest, TriggersIPH) {
  ReopenTabInProductHelp reopen_tab_iph(profile(), clock());

  auto* mock_tracker = GetMockTracker();
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
      .Times(1)
      .WillOnce(Return(true));

  AddTab(browser(), GURL("chrome://blank"));
  AddTab(browser(), GURL("chrome://blank"));
  BrowserList::SetLastActive(browser());

  clock()->Advance(kTabMinimumActiveDuration);

  auto* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ToggleSelectionAt(0);
  ASSERT_TRUE(tab_strip_model->IsTabSelected(0));
  tab_strip_model->CloseSelectedTabs();

  reopen_tab_iph.NewTabOpened();
}
