// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/ios/notice_card_tracker.h"

#include "components/feed/core/common/pref_names.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace ios_feed {

constexpr char kNoticeCardViewsCountThresholdParamName[] =
    "notice-card-views-count-threshold";
constexpr char kNoticeCardClicksCountThresholdParamName[] =
    "notice-card-clicks-count-threshold";

NoticeCardTracker::NoticeCardTracker(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
  views_count_ = feed::prefs::GetNoticeCardViewsCount(*profile_prefs_);
  clicks_count_ = feed::prefs::GetNoticeCardClicksCount(*profile_prefs_);

  views_count_threshold_ = base::GetFieldTrialParamByFeatureAsInt(
      feed::kInterestFeedNoticeCardAutoDismiss,
      kNoticeCardViewsCountThresholdParamName, 3);
  DCHECK(views_count_threshold_ >= 0);

  clicks_count_threshold_ = base::GetFieldTrialParamByFeatureAsInt(
      feed::kInterestFeedNoticeCardAutoDismiss,
      kNoticeCardClicksCountThresholdParamName, 1);
  DCHECK(clicks_count_threshold_ >= 0);

  DCHECK(views_count_threshold_ > 0 || clicks_count_threshold_ > 0)
      << "all notice card auto-dismiss thresholds are set to 0 when there "
         "should be at least one threshold above 0";
}

void NoticeCardTracker::OnSliceViewed(int index) {
  MaybeUpdateNoticeCardViewsCount(index);
}

void NoticeCardTracker::OnOpenAction(int index) {
  MaybeUpdateNoticeCardClicksCount(index);
}

bool NoticeCardTracker::HasAcknowledgedNoticeCard() const {
  if (!base::FeatureList::IsEnabled(feed::kInterestFeedNoticeCardAutoDismiss))
    return false;

  base::AutoLock auto_lock_views(views_count_lock_);
  if (views_count_threshold_ > 0 && views_count_ >= views_count_threshold_) {
    return true;
  }

  base::AutoLock auto_lock_clicks(clicks_count_lock_);
  if (clicks_count_threshold_ > 0 && clicks_count_ >= clicks_count_threshold_) {
    return true;
  }

  return false;
}

bool NoticeCardTracker::HasNoticeCardActionsCountPrerequisites(int index) {
  if (!base::FeatureList::IsEnabled(feed::kInterestFeedNoticeCardAutoDismiss)) {
    return false;
  }

  if (!feed::prefs::GetLastFetchHadNoticeCard(*profile_prefs_)) {
    return false;
  }

  return index == 0;
}

void NoticeCardTracker::MaybeUpdateNoticeCardViewsCount(int index) {
  if (!HasNoticeCardActionsCountPrerequisites(index))
    return;

  feed::prefs::IncrementNoticeCardViewsCount(*profile_prefs_);
  base::AutoLock auto_lock(views_count_lock_);
  views_count_++;
}

void NoticeCardTracker::MaybeUpdateNoticeCardClicksCount(int index) {
  if (!HasNoticeCardActionsCountPrerequisites(index))
    return;

  feed::prefs::IncrementNoticeCardClicksCount(*profile_prefs_);
  base::AutoLock auto_lock(clicks_count_lock_);
  clicks_count_++;
}

}  // namespace ios_feed
