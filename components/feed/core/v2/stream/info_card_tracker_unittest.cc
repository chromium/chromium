// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/info_card_tracker.h"

#include <string>
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feedstore_util.h"
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
  EXPECT_TRUE(tracker_.GetAllStates(0, 0).empty());
}

TEST_F(InfoCardTrackerTest, ViewCount) {
  int64_t initial_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  int64_t view_timestamp = initial_timestamp;
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(1);
  expected_state.set_first_view_timestamp(view_timestamp);
  expected_state.set_last_view_timestamp(view_timestamp);
  EXPECT_THAT(tracker_.GetAllStates(initial_timestamp, initial_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));

  // No enough time. The view count does not increase.
  task_environment_.AdvanceClock(kNotEnoughTime);
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  EXPECT_THAT(tracker_.GetAllStates(initial_timestamp, initial_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));

  // Enough time. The view count increases.
  task_environment_.AdvanceClock(kEnoughTime - kNotEnoughTime);
  view_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  expected_state.set_view_count(2);
  expected_state.set_last_view_timestamp(view_timestamp);
  EXPECT_THAT(tracker_.GetAllStates(initial_timestamp, initial_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));
}

TEST_F(InfoCardTrackerTest, ResetState) {
  int64_t initial_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  int64_t view_timestamp = initial_timestamp;
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  tracker_.OnClicked(kTestInfoCardType1);
  tracker_.OnDismissed(kTestInfoCardType1);
  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(1);
  expected_state.set_click_count(1);
  expected_state.set_explicitly_dismissed_count(1);
  expected_state.set_first_view_timestamp(view_timestamp);
  expected_state.set_last_view_timestamp(view_timestamp);
  EXPECT_THAT(tracker_.GetAllStates(initial_timestamp, initial_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));

  tracker_.ResetState(kTestInfoCardType1);
  expected_state.Clear();
  expected_state.set_type(kTestInfoCardType1);
  EXPECT_THAT(tracker_.GetAllStates(initial_timestamp, initial_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));
}

TEST_F(InfoCardTrackerTest, ComboActions) {
  int64_t initial_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  int64_t type1_fisrt_view_time = initial_timestamp;
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  tracker_.OnClicked(kTestInfoCardType1);
  tracker_.OnDismissed(kTestInfoCardType1);

  task_environment_.AdvanceClock(kEnoughTime);
  tracker_.OnClicked(kTestInfoCardType2);
  tracker_.OnClicked(kTestInfoCardType2);
  int64_t type2_fisrt_view_time =
      feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType2, kMinimumViewIntervalSeconds);
  task_environment_.AdvanceClock(kNotEnoughTime);
  tracker_.OnViewed(kTestInfoCardType2, kMinimumViewIntervalSeconds);

  task_environment_.AdvanceClock(kEnoughTime);
  int64_t type1_last_view_time =
      feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);
  tracker_.OnClicked(kTestInfoCardType1);

  feedwire::InfoCardTrackingState expected_state1;
  expected_state1.set_type(kTestInfoCardType1);
  expected_state1.set_view_count(2);
  expected_state1.set_click_count(2);
  expected_state1.set_explicitly_dismissed_count(1);
  expected_state1.set_first_view_timestamp(type1_fisrt_view_time);
  expected_state1.set_last_view_timestamp(type1_last_view_time);
  feedwire::InfoCardTrackingState expected_state2;
  expected_state2.set_type(kTestInfoCardType2);
  expected_state2.set_view_count(1);
  expected_state2.set_click_count(2);
  expected_state2.set_first_view_timestamp(type2_fisrt_view_time);
  expected_state2.set_last_view_timestamp(type2_fisrt_view_time);
  EXPECT_THAT(tracker_.GetAllStates(initial_timestamp, initial_timestamp),
              testing::ElementsAre(EqualsProto(expected_state1),
                                   EqualsProto(expected_state2)));

  tracker_.ResetState(kTestInfoCardType1);
  expected_state1.Clear();
  expected_state1.set_type(kTestInfoCardType1);
  EXPECT_THAT(tracker_.GetAllStates(initial_timestamp, initial_timestamp),
              testing::ElementsAre(EqualsProto(expected_state1),
                                   EqualsProto(expected_state2)));
}

TEST_F(InfoCardTrackerTest, AdjustViewTimestamp_ServerTimestampLessThanClient) {
  // Make server timestamp earlier than client timestamp.
  int64_t server_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  task_environment_.AdvanceClock(base::Minutes(10));
  int64_t client_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  task_environment_.AdvanceClock(base::Minutes(2));

  int64_t first_view_timestamp =
      feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  task_environment_.AdvanceClock(kEnoughTime);
  int64_t last_view_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(2);
  // Standard adjustment should be made to view timestamps.
  expected_state.set_first_view_timestamp(first_view_timestamp +
                                          server_timestamp - client_timestamp);
  expected_state.set_last_view_timestamp(last_view_timestamp +
                                         server_timestamp - client_timestamp);
  EXPECT_THAT(tracker_.GetAllStates(server_timestamp, client_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));
}

TEST_F(InfoCardTrackerTest, AdjustViewTimestamp_ClientTimestampLessThanServer) {
  // Make client timestamp earlier than server timestamp.
  int64_t client_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  task_environment_.AdvanceClock(base::Minutes(10));
  int64_t server_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  task_environment_.AdvanceClock(base::Minutes(2));

  int64_t first_view_timestamp =
      feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  task_environment_.AdvanceClock(kEnoughTime);
  int64_t last_view_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(2);
  // Standard adjustment should be made to view timestamps.
  expected_state.set_first_view_timestamp(first_view_timestamp +
                                          server_timestamp - client_timestamp);
  expected_state.set_last_view_timestamp(last_view_timestamp +
                                         server_timestamp - client_timestamp);
  EXPECT_THAT(tracker_.GetAllStates(server_timestamp, client_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));
}

TEST_F(InfoCardTrackerTest,
       AdjustViewTimestamp_DoNotGoEarlierThanServerTimestamp) {
  int64_t server_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  task_environment_.AdvanceClock(base::Minutes(10));
  // Move client timestamp to some time later to simulate the client clock
  // going backward.
  int64_t client_timestamp =
      feedstore::ToTimestampMillis(base::Time::Now() + base::Hours(1));

  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  task_environment_.AdvanceClock(kEnoughTime);
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(2);
  // The view timestamps should not go earlier than server timestamp.
  expected_state.set_first_view_timestamp(server_timestamp);
  expected_state.set_last_view_timestamp(server_timestamp);
  EXPECT_THAT(tracker_.GetAllStates(server_timestamp, client_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));
}

TEST_F(InfoCardTrackerTest, AdjustViewTimestamp_DoNotGoOverContentLifetime) {
  // Make server timestamp earlier than client timestamp.
  int64_t server_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  task_environment_.AdvanceClock(base::Minutes(10));
  int64_t client_timestamp = feedstore::ToTimestampMillis(base::Time::Now());
  task_environment_.AdvanceClock(base::Minutes(2));

  int64_t first_view_timestamp =
      feedstore::ToTimestampMillis(base::Time::Now());
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  task_environment_.AdvanceClock(GetFeedConfig().content_expiration_threshold +
                                 base::Minutes(20));
  tracker_.OnViewed(kTestInfoCardType1, kMinimumViewIntervalSeconds);

  feedwire::InfoCardTrackingState expected_state;
  expected_state.set_type(kTestInfoCardType1);
  expected_state.set_view_count(2);
  // Standard adjustment should be made to view timestamps.
  expected_state.set_first_view_timestamp(first_view_timestamp +
                                          server_timestamp - client_timestamp);
  // View timestamp cannot go over content's lifetime.
  expected_state.set_last_view_timestamp(
      server_timestamp +
      GetFeedConfig().content_expiration_threshold.InMilliseconds());
  EXPECT_THAT(tracker_.GetAllStates(server_timestamp, client_timestamp),
              testing::ElementsAre(EqualsProto(expected_state)));
}

}  // namespace feed
