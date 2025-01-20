// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/ios/notice_card_tracker.h"

#include "components/feed/core/common/pref_names.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace ios_feed {

namespace {

// The number of views of the notice card to consider it acknowledged by the
// user.
const int kViewsCountThreshold = 3;

// The number of clicks/taps of the notice card to consider it acknowledged by
// the user.
const int kClicksCountThreshold = 1;

}  // namespace

NoticeCardTracker::NoticeCardTracker(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
  views_count_ = feed::prefs::GetNoticeCardViewsCount(*profile_prefs_);
  clicks_count_ = feed::prefs::GetNoticeCardClicksCount(*profile_prefs_);
}

void NoticeCardTracker::OnSliceViewed(int index) {
  MaybeUpdateNoticeCardViewsCount(index);
}

void NoticeCardTracker::OnOpenAction(int index) {
  MaybeUpdateNoticeCardClicksCount(index);
}

bool NoticeCardTracker::HasAcknowledgedNoticeCard() const {
  base::AutoLock auto_lock_views(views_count_lock_);
  if (views_count_ >= kViewsCountThreshold) {
    return true;
  }

  base::AutoLock auto_lock_clicks(clicks_count_lock_);
  if (clicks_count_ >= kClicksCountThreshold) {
    return true;
  }

  return false;
}

bool NoticeCardTracker::HasNoticeCardActionsCountPrerequisites(int index) {
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
