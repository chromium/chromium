// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_NOTICE_CARD_TRACKER_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_NOTICE_CARD_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
// TODO(crbug.com/40768780): Remove this include:
#include "components/feed/core/v2/public/ios/prefs.h"

class PrefService;

namespace ios_feed {

// Tracker for the notice card related actions that also provide signals based
// on those.
class NoticeCardTracker {
 public:
  explicit NoticeCardTracker(PrefService* profile_prefs);

  NoticeCardTracker(const NoticeCardTracker&) = delete;
  NoticeCardTracker& operator=(const NoticeCardTracker&) = delete;

  // Capture the actions.

  void OnSliceViewed(int index);
  void OnOpenAction(int index);

  // Get signals based on the actions.

  // Indicates whether there were enough views or clicks done on the notice
  // card to consider it as acknowledged by the user. This is safe to call in a
  // background thread.
  bool HasAcknowledgedNoticeCard() const;

 private:
  bool HasNoticeCardActionsCountPrerequisites(int index);
  void MaybeUpdateNoticeCardViewsCount(int index);
  void MaybeUpdateNoticeCardClicksCount(int index);

  raw_ptr<PrefService> profile_prefs_;

  // The number of views of the notice card.
  mutable base::Lock views_count_lock_;
  int views_count_ GUARDED_BY(views_count_lock_);

  // The number of clicks/taps of the notice card.
  mutable base::Lock clicks_count_lock_;
  int clicks_count_ GUARDED_BY(clicks_count_lock_);

  // The number of views of the notice card to consider it acknowledged by the
  // user.
  int views_count_threshold_;

  // The number of clicks/taps of the notice card to consider it acknowledged by
  // the user.
  int clicks_count_threshold_;
};

}  // namespace ios_feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_NOTICE_CARD_TRACKER_H_
