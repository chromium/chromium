// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/notice_card_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

feedwire::ContentId NoticeCardContentId() {
  feedwire::ContentId id;
  id.set_content_domain("privacynoticecard.f");
  return id;
}

feedwire::ContentId OtherContentId() {
  feedwire::ContentId id;
  id.set_content_domain("NOTprivacynoticecard.f");
  return id;
}

class NoticeCardTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    // Make sure current time isn't zero.
    task_environment_.AdvanceClock(base::TimeDelta::FromDays(1));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
};

TEST_F(NoticeCardTrackerTest,
       TrackingNoticeCardActionsDoesntUpdateCountsForNonNoticeCard) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  // Generate enough views to reach the acknowlegement threshold, but the views
  // were not on the notice card.
  tracker.OnCardViewed(/*is_signed_in=*/true, OtherContentId());
  task_environment_.AdvanceClock(base::TimeDelta::FromMinutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, OtherContentId());
  task_environment_.AdvanceClock(base::TimeDelta::FromMinutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, OtherContentId());

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest, AcknowledgedNoticeCardWhenEnoughViews) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::TimeDelta::FromMinutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::TimeDelta::FromMinutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());

  EXPECT_TRUE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest, ViewsAreIgnoredIfNotEnoughTimeElapsed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::TimeDelta::FromMinutes(4));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::TimeDelta::FromMinutes(4));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest,
       DontAcknowledgedNoticeCardWhenNotEnoughViewsNorClicks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  // Generate views but not enough to reach the threshold.
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());
  task_environment_.AdvanceClock(base::TimeDelta::FromMinutes(6));
  tracker.OnCardViewed(/*is_signed_in=*/true, NoticeCardContentId());

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest, DontAcknowledgedNoticeCardWhenFeatureDisabled) {
  // Generate enough views and clicks on the notice card to reach the threshold,
  // but the feature is disabled.
  prefs::IncrementNoticeCardClicksCount(profile_prefs_);
  prefs::IncrementNoticeCardViewsCount(profile_prefs_);
  prefs::IncrementNoticeCardViewsCount(profile_prefs_);
  prefs::IncrementNoticeCardViewsCount(profile_prefs_);

  NoticeCardTracker tracker(&profile_prefs_);
  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest,
       DontAcknowledgedNoticeCardFromViewsCountWhenThresholdIsZero) {
  base::FieldTrialParams params;
  params[kNoticeCardViewsCountThresholdParamName] = "0";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      feed::kInterestFeedNoticeCardAutoDismiss, params);

  NoticeCardTracker tracker(&profile_prefs_);
  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest,
       DontAcknowledgedNoticeCardFromClicksCountWhenThresholdIsZero) {
  base::FieldTrialParams params;
  params[kNoticeCardClicksCountThresholdParamName] = "0";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      feed::kInterestFeedNoticeCardAutoDismiss, params);

  NoticeCardTracker tracker(&profile_prefs_);
  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

}  // namespace
}  // namespace feed
