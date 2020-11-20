// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/notice_card_tracker.h"

#include "components/feed/core/v2/prefs.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {

int GetNoticeCardIndex() {
  // Infer that the notice card is at the 2nd position when the feature related
  // to putting the notice card at the second position is enabled.
  if (base::FeatureList::IsEnabled(
          feed::kInterestFeedV2ClicksAndViewsConditionalUpload)) {
    return 1;
  }
  return 0;
}

}  // namespace

NoticeCardTracker::NoticeCardTracker(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
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

  int views_count_threshold = base::GetFieldTrialParamByFeatureAsInt(
      feed::kInterestFeedNoticeCardAutoDismiss,
      kNoticeCardViewsCountThresholdParamName, 3);
  DCHECK(views_count_threshold >= 0);
  int clicks_count_threshold = base::GetFieldTrialParamByFeatureAsInt(
      feed::kInterestFeedNoticeCardAutoDismiss,
      kNoticeCardClicksCountThresholdParamName, 1);
  DCHECK(clicks_count_threshold >= 0);

  DCHECK(views_count_threshold > 0 || clicks_count_threshold > 0)
      << "all notice card auto-dismiss thresholds are set to 0 when there "
         "should be at least one threshold above 0";

  if (views_count_threshold > 0 &&
      prefs::GetNoticeCardViewsCount(*profile_prefs_) >=
          views_count_threshold) {
    return true;
  }

  if (clicks_count_threshold > 0 &&
      prefs::GetNoticeCardClicksCount(*profile_prefs_) >=
          clicks_count_threshold) {
    return true;
  }

  return false;
}

bool NoticeCardTracker::HasNoticeCardActionsCountPrerequisites(int index) {
  if (!base::FeatureList::IsEnabled(feed::kInterestFeedNoticeCardAutoDismiss))
    return false;

  if (!prefs::GetLastFetchHadNoticeCard(*profile_prefs_)) {
    return false;
  }

  if (index != GetNoticeCardIndex()) {
    return false;
  }
  return true;
}
void NoticeCardTracker::MaybeUpdateNoticeCardViewsCount(int index) {
  if (!HasNoticeCardActionsCountPrerequisites(index))
    return;

  prefs::IncrementNoticeCardViewsCount(*profile_prefs_);
}
void NoticeCardTracker::MaybeUpdateNoticeCardClicksCount(int index) {
  if (!HasNoticeCardActionsCountPrerequisites(index))
    return;

  prefs::IncrementNoticeCardClicksCount(*profile_prefs_);
}

}  // namespace feed