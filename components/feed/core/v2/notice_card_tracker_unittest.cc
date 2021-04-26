// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/notice_card_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

class NoticeCardTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    ;
  }

 protected:
  TestingPrefServiceSimple profile_prefs_;
};

TEST_F(NoticeCardTrackerTest,
       TrackingNoticeCardActionsDoesntUpdateCountsWhenNoNoticeCard) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  prefs::SetLastFetchHadNoticeCard(profile_prefs_, false);

  // Generate enough views to reach the acknowlegement threshold, but there was
  // no notice card in the feed.
  const int notice_card_index = 0;
  tracker.OnSliceViewed(notice_card_index);
  tracker.OnSliceViewed(notice_card_index);
  tracker.OnSliceViewed(notice_card_index);

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest,
       TrackingNoticeCardActionsDoesntUpdateCountsForNonNoticeCard) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  // Generate enough views to reach the acknowlegement threshold, but the views
  // were not on the notice card.
  const int non_notice_card_index = 1;
  tracker.OnSliceViewed(non_notice_card_index);
  tracker.OnSliceViewed(non_notice_card_index);
  tracker.OnSliceViewed(non_notice_card_index);

  EXPECT_FALSE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest,
       AcknowledgedNoticeCardWhenEnoughViewsAndNoticeCardAt1stPos) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  const int notice_card_index = 0;
  tracker.OnSliceViewed(notice_card_index);
  tracker.OnSliceViewed(notice_card_index);
  tracker.OnSliceViewed(notice_card_index);

  EXPECT_TRUE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest,
       AcknowledgedNoticeCardWhenEnoughViewsAndNoticeCardAt2ndPos) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{feed::kInterestFeedNoticeCardAutoDismiss,
                            feed::
                                kInterestFeedV2ClicksAndViewsConditionalUpload},
      /*disabled_features=*/{});
  NoticeCardTracker tracker(&profile_prefs_);

  const int notice_card_index = 1;
  tracker.OnSliceViewed(notice_card_index);
  tracker.OnSliceViewed(notice_card_index);
  tracker.OnSliceViewed(notice_card_index);

  EXPECT_TRUE(tracker.HasAcknowledgedNoticeCard());
}

TEST_F(NoticeCardTrackerTest,
       DontAcknowledgedNoticeCardWhenNotEnoughViewsNorClicks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);
  NoticeCardTracker tracker(&profile_prefs_);

  // Generate views but not enough to reach the threshold.
  const int notice_card_index = 0;
  tracker.OnSliceViewed(notice_card_index);
  tracker.OnSliceViewed(notice_card_index);

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
