// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/notice_card_tracker.h"

#include <string>
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const std::string kTestKey = "test";
const std::string kTestKey2 = "foo";
const std::string kTestKey3 = "hello";

class NoticeCardTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    // Make sure current time isn't zero.
    task_environment_.AdvanceClock(base::Days(1));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Note that kEnoughTime should not be declared as global vairiable since
  // otherwise it will affect other tests that depend on the overridden
  // GetFeedConfig().
  base::TimeDelta kEnoughTime = GetFeedConfig().minimum_notice_view_interval;
  const base::TimeDelta kNotEnoughTime = kEnoughTime - base::Minutes(1);
  TestingPrefServiceSimple profile_prefs_;
  NoticeCardTracker tracker1_{&profile_prefs_, kTestKey};
  NoticeCardTracker tracker2_{&profile_prefs_, kTestKey2};
  NoticeCardTracker tracker3_{&profile_prefs_, kTestKey3};
};

TEST_F(NoticeCardTrackerTest, AcknowledgedNoticeCardWhenEnoughViews) {
  for (int i = 0; i < NoticeCardTracker::kViewCountThreshold; ++i) {
    if (i > 0)
      task_environment_.AdvanceClock(kEnoughTime);
    tracker1_.OnViewed();
  }

  EXPECT_TRUE(tracker1_.HasAcknowledged());
}

TEST_F(NoticeCardTrackerTest, AcknowledgedNoticeCardWhenEnoughClicks) {
  for (int i = 0; i < NoticeCardTracker::kClickCountThreshold; ++i) {
    tracker1_.OnOpenAction();
  }

  EXPECT_TRUE(tracker1_.HasAcknowledged());
}

TEST_F(NoticeCardTrackerTest, ViewsAreIgnoredIfNotEnoughTimeElapsed) {
  for (int i = 0; i < NoticeCardTracker::kViewCountThreshold; ++i) {
    if (i > 0)
      task_environment_.AdvanceClock(kNotEnoughTime);
    tracker1_.OnViewed();
  }

  EXPECT_FALSE(tracker1_.HasAcknowledged());
}

TEST_F(NoticeCardTrackerTest,
       DontAcknowledgedNoticeCardWhenNotEnoughViewsNorClicks) {
  for (int i = 0; i < NoticeCardTracker::kViewCountThreshold - 1; ++i) {
    if (i > 0)
      task_environment_.AdvanceClock(kEnoughTime);
    tracker1_.OnViewed();
  }

  EXPECT_FALSE(tracker1_.HasAcknowledged());
}

TEST_F(NoticeCardTrackerTest, Dismiss) {
  tracker2_.OnDismissed();
  EXPECT_FALSE(tracker1_.HasAcknowledged());
  EXPECT_TRUE(tracker2_.HasAcknowledged());
  EXPECT_FALSE(tracker3_.HasAcknowledged());

  std::vector<std::string> acknowledged_keys =
      NoticeCardTracker::GetAllAckowledgedKeys(&profile_prefs_);
  EXPECT_THAT(acknowledged_keys, testing::UnorderedElementsAre(kTestKey2));
}

TEST_F(NoticeCardTrackerTest, MultipleKeysForViews) {
  for (int i = 0; i < NoticeCardTracker::kViewCountThreshold - 1; ++i) {
    if (i > 0)
      task_environment_.AdvanceClock(kEnoughTime);
    tracker1_.OnViewed();
    tracker2_.OnViewed();
  }

  // Only make |tracker1_| get enough views.
  task_environment_.AdvanceClock(kEnoughTime);
  tracker1_.OnViewed();

  EXPECT_TRUE(tracker1_.HasAcknowledged());
  EXPECT_FALSE(tracker2_.HasAcknowledged());
  EXPECT_FALSE(tracker3_.HasAcknowledged());

  std::vector<std::string> acknowledged_keys =
      NoticeCardTracker::GetAllAckowledgedKeys(&profile_prefs_);
  EXPECT_THAT(acknowledged_keys, testing::UnorderedElementsAre(kTestKey));
}

TEST_F(NoticeCardTrackerTest, MultipleKeysForClicks) {
  for (int i = 0; i < NoticeCardTracker::kClickCountThreshold; ++i) {
    tracker1_.OnOpenAction();
    tracker2_.OnOpenAction();
  }

  EXPECT_TRUE(tracker1_.HasAcknowledged());
  EXPECT_TRUE(tracker2_.HasAcknowledged());
  EXPECT_FALSE(tracker3_.HasAcknowledged());

  std::vector<std::string> acknowledged_keys =
      NoticeCardTracker::GetAllAckowledgedKeys(&profile_prefs_);
  EXPECT_THAT(acknowledged_keys,
              testing::UnorderedElementsAre(kTestKey, kTestKey2));
}

TEST_F(NoticeCardTrackerTest, Combo) {
  std::vector<std::string> acknowledged_keys;

  // Make one card have enough views.
  for (int i = 0; i < NoticeCardTracker::kViewCountThreshold; ++i) {
    if (i > 0)
      task_environment_.AdvanceClock(kEnoughTime);
    tracker2_.OnViewed();
  }

  EXPECT_FALSE(tracker1_.HasAcknowledged());
  EXPECT_TRUE(tracker2_.HasAcknowledged());
  EXPECT_FALSE(tracker3_.HasAcknowledged());

  acknowledged_keys = NoticeCardTracker::GetAllAckowledgedKeys(&profile_prefs_);
  EXPECT_THAT(acknowledged_keys, testing::UnorderedElementsAre(kTestKey2));

  // Make another card have enough clicks;
  for (int i = 0; i < NoticeCardTracker::kClickCountThreshold; ++i) {
    tracker3_.OnOpenAction();
  }

  EXPECT_FALSE(tracker1_.HasAcknowledged());
  EXPECT_TRUE(tracker2_.HasAcknowledged());
  EXPECT_TRUE(tracker3_.HasAcknowledged());

  acknowledged_keys = NoticeCardTracker::GetAllAckowledgedKeys(&profile_prefs_);
  EXPECT_THAT(acknowledged_keys,
              testing::UnorderedElementsAre(kTestKey2, kTestKey3));

  // Dismiss some other card.
  tracker1_.OnDismissed();

  EXPECT_TRUE(tracker1_.HasAcknowledged());
  EXPECT_TRUE(tracker2_.HasAcknowledged());
  EXPECT_TRUE(tracker3_.HasAcknowledged());

  acknowledged_keys = NoticeCardTracker::GetAllAckowledgedKeys(&profile_prefs_);
  EXPECT_THAT(acknowledged_keys,
              testing::UnorderedElementsAre(kTestKey, kTestKey2, kTestKey3));
}

}  // namespace
}  // namespace feed
