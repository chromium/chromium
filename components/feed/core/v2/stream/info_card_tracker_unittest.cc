// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/info_card_tracker.h"

#include <string>
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/test/test_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

const int kMinimumViewIntervalSeconds = 5 * 60;
const base::TimeDelta kEnoughTime = base::Seconds(kMinimumViewIntervalSeconds);
const base::TimeDelta kNotEnoughTime = kEnoughTime - base::Minutes(1);
const feedwire::InfoCardType kTestInfoCardType1 =
    feedwire::INFO_CARD_MAIN_PRIVACY_NOTICE;
const feedwire::InfoCardType kTestInfoCardType2 =
    feedwire::INFO_CARD_YOUTUBE_PRIVACY_NOTICE;

}  // namespace

class InfoCardTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    // Make sure current time isn't zero.
    task_environment_.AdvanceClock(base::Days(1));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
  InfoCardTracker tracker_{&profile_prefs_};
};

TEST_F(InfoCardTrackerTest, InitialEmptyState) {
  EXPECT_TRUE(tracker_.GetAllStates().empty());
}

TEST_F(InfoCardTrackerTest, ViewCount) {
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(1);
  EXPECT_THAT(tracker_.GetAllStates(),
              testing::ElementsAre(EqualsProto(expected_state)));

  // No enough time. The view count does not increase.
  task_environment_.AdvanceClock(kNotEnoughTime);
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  EXPECT_THAT(tracker_.GetAllStates(),
              testing::ElementsAre(EqualsProto(expected_state)));

  // Enough time. The view count increases.
  task_environment_.AdvanceClock(kEnoughTime - kNotEnoughTime);
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  expected_state.set_view_count(2);
  EXPECT_THAT(tracker_.GetAllStates(),
              testing::ElementsAre(EqualsProto(expected_state)));
}

TEST_F(InfoCardTrackerTest, ResetState) {
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  tracker_.OnClicked(kTestInfoCardType1);
  tracker_.OnDismissed(kTestInfoCardType1);
  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(1);
  expected_state.set_click_count(1);
  expected_state.set_explicitly_dismissed_count(1);
  EXPECT_THAT(tracker_.GetAllStates(),
              testing::ElementsAre(EqualsProto(expected_state)));

  tracker_.ResetState(kTestInfoCardType1);
  expected_state.Clear();
  expected_state.set_type(kTestInfoCardType1);
  EXPECT_THAT(tracker_.GetAllStates(),
              testing::ElementsAre(EqualsProto(expected_state)));
}

TEST_F(InfoCardTrackerTest, ComboActions) {
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  tracker_.OnClicked(kTestInfoCardType1);
  tracker_.OnDismissed(kTestInfoCardType1);

  tracker_.OnClicked(kTestInfoCardType2);
  tracker_.OnClicked(kTestInfoCardType2);
  tracker_.OnViewed(kTestInfoCardType2, kMinimumViewIntervalSeconds);
  task_environment_.AdvanceClock(kNotEnoughTime);
  tracker_.OnViewed(kTestInfoCardType2, kMinimumViewIntervalSeconds);

  task_environment_.AdvanceClock(kEnoughTime);
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  tracker_.OnClicked(kTestInfoCardType1);

  feedwire::InfoCardTrackingState expected_state1;
  expected_state1.set_type(kTestInfoCardType1);
  expected_state1.set_view_count(2);
  expected_state1.set_click_count(2);
  expected_state1.set_explicitly_dismissed_count(1);
  feedwire::InfoCardTrackingState expected_state2;
  expected_state2.set_type(kTestInfoCardType2);
  expected_state2.set_view_count(1);
  expected_state2.set_click_count(2);
  EXPECT_THAT(tracker_.GetAllStates(),
              testing::ElementsAre(EqualsProto(expected_state1),
                                   EqualsProto(expected_state2)));

  tracker_.ResetState(kTestInfoCardType1);
  expected_state1.Clear();
  expected_state1.set_type(kTestInfoCardType1);
  EXPECT_THAT(tracker_.GetAllStates(),
              testing::ElementsAre(EqualsProto(expected_state1),
                                   EqualsProto(expected_state2)));
}

}  // namespace feed
