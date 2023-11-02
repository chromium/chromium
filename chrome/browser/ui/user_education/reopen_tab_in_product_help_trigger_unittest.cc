// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/reopen_tab_in_product_help_trigger.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

using feature_engagement::test::MockTracker;

namespace {

// Triggering timeouts that will be passed through Finch params. They are chosen
// to be different than the fallback timeouts to test that
// ReopenTabInProductHelpTrigger does use the params if they exist.
constexpr base::TimeDelta kTabMinimumActiveDuration = base::Seconds(15);
constexpr base::TimeDelta kNewTabOpenedTimeout = base::Seconds(5);

}  // namespace

class ReopenTabInProductHelpTriggerTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHReopenTabFeature,
        ReopenTabInProductHelpTrigger::GetFieldTrialParamsForTest(
            kTabMinimumActiveDuration.InSeconds(),
            kNewTabOpenedTimeout.InSeconds()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReopenTabInProductHelpTriggerTest, TriggersIPH) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(
      mock_tracker,
      NotifyEvent(Eq(feature_engagement::events::kReopenTabConditionsMet)))
      .Times(1);

  // Instantiate IPH and send sequence of user interactions.
  base::SimpleTestTickClock clock;
  ReopenTabInProductHelpTrigger reopen_tab_iph(&mock_tracker, &clock);

  base::MockCallback<base::RepeatingCallback<void()>> show_help_callback;
  reopen_tab_iph.SetShowHelpCallback(show_help_callback.Get());

  reopen_tab_iph.ActiveTabClosed(kTabMinimumActiveDuration);
  EXPECT_CALL(show_help_callback, Run());
  reopen_tab_iph.NewTabOpened();
}

TEST_F(ReopenTabInProductHelpTriggerTest, AlwaysCallsBackendOnNewTab) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(
      mock_tracker,
      NotifyEvent(Eq(feature_engagement::events::kReopenTabConditionsMet)))
      .Times(0);

  base::SimpleTestTickClock clock;
  ReopenTabInProductHelpTrigger reopen_tab_iph(&mock_tracker, &clock);

  base::MockCallback<base::RepeatingCallback<void()>> show_help_callback;
  reopen_tab_iph.SetShowHelpCallback(show_help_callback.Get());

  EXPECT_CALL(show_help_callback, Run());
  // Opening a new tab without closing an active tab first:
  reopen_tab_iph.NewTabOpened();

  EXPECT_CALL(show_help_callback, Run());
  // Opening a new tab after closing a tab too quickly:
  reopen_tab_iph.ActiveTabClosed(kTabMinimumActiveDuration / 2);
  reopen_tab_iph.NewTabOpened();
}

TEST_F(ReopenTabInProductHelpTriggerTest, TabNotActiveLongEnough) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(mock_tracker, NotifyEvent(_)).Times(0);

  base::SimpleTestTickClock clock;
  ReopenTabInProductHelpTrigger reopen_tab_iph(&mock_tracker, &clock);

  reopen_tab_iph.SetShowHelpCallback(base::DoNothing());

  reopen_tab_iph.ActiveTabClosed(kTabMinimumActiveDuration / 2);
  reopen_tab_iph.NewTabOpened();
}

TEST_F(ReopenTabInProductHelpTriggerTest, RespectsTimeout) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(mock_tracker, NotifyEvent(_)).Times(0);

  base::SimpleTestTickClock clock;
  ReopenTabInProductHelpTrigger reopen_tab_iph(&mock_tracker, &clock);

  reopen_tab_iph.SetShowHelpCallback(base::DoNothing());

  reopen_tab_iph.ActiveTabClosed(kTabMinimumActiveDuration);
  clock.Advance(kNewTabOpenedTimeout);
  reopen_tab_iph.NewTabOpened();
}

TEST_F(ReopenTabInProductHelpTriggerTest, TriggersTwice) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(
      mock_tracker,
      NotifyEvent(Eq(feature_engagement::events::kReopenTabConditionsMet)))
      .Times(2);

  base::SimpleTestTickClock clock;
  ReopenTabInProductHelpTrigger reopen_tab_iph(&mock_tracker, &clock);

  bool triggered = false;
  reopen_tab_iph.SetShowHelpCallback(
      base::BindLambdaForTesting([&triggered]() { triggered = true; }));

  reopen_tab_iph.ActiveTabClosed(kTabMinimumActiveDuration);
  reopen_tab_iph.NewTabOpened();

  EXPECT_TRUE(triggered);
  triggered = false;

  reopen_tab_iph.ActiveTabClosed(kTabMinimumActiveDuration);
  reopen_tab_iph.NewTabOpened();

  EXPECT_TRUE(triggered);
}
