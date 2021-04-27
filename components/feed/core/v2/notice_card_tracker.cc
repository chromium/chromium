// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/notice_card_tracker.h"

#include "components/feed/core/common/pref_names.h"
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

namespace prefs {

void IncrementNoticeCardViewsCount(PrefService& pref_service) {
  int count = pref_service.GetInteger(feed::prefs::kNoticeCardViewsCount);
  pref_service.SetInteger(feed::prefs::kNoticeCardViewsCount, count + 1);
}

int GetNoticeCardViewsCount(const PrefService& pref_service) {
  return pref_service.GetInteger(feed::prefs::kNoticeCardViewsCount);
}

void IncrementNoticeCardClicksCount(PrefService& pref_service) {
  int count = pref_service.GetInteger(feed::prefs::kNoticeCardClicksCount);
  pref_service.SetInteger(feed::prefs::kNoticeCardClicksCount, count + 1);
}

int GetNoticeCardClicksCount(const PrefService& pref_service) {
  return pref_service.GetInteger(feed::prefs::kNoticeCardClicksCount);
}

void SetLastFetchHadNoticeCard(PrefService& pref_service, bool value) {
  pref_service.SetBoolean(feed::prefs::kLastFetchHadNoticeCard, value);
}

bool GetLastFetchHadNoticeCard(const PrefService& pref_service) {
  return pref_service.GetBoolean(feed::prefs::kLastFetchHadNoticeCard);
}

void SetHasReachedClickAndViewActionsUploadConditions(PrefService& pref_service,
                                                      bool value) {
  pref_service.SetBoolean(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions, value);
}

bool GetHasReachedClickAndViewActionsUploadConditions(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions);
}

}  // namespace prefs

NoticeCardTracker::NoticeCardTracker(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
  views_count_ = prefs::GetNoticeCardViewsCount(*profile_prefs_);
  clicks_count_ = prefs::GetNoticeCardClicksCount(*profile_prefs_);

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
  base::AutoLock auto_lock(views_count_lock_);
  views_count_++;
}

void NoticeCardTracker::MaybeUpdateNoticeCardClicksCount(int index) {
  if (!HasNoticeCardActionsCountPrerequisites(index))
    return;

  prefs::IncrementNoticeCardClicksCount(*profile_prefs_);
  base::AutoLock auto_lock(clicks_count_lock_);
  clicks_count_++;
}

}  // namespace feed
