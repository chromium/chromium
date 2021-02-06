// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_NOTICE_CARD_TRACKER_H_
#define COMPONENTS_FEED_CORE_V2_NOTICE_CARD_TRACKER_H_

class PrefService;

namespace feed {

constexpr char kNoticeCardViewsCountThresholdParamName[] =
    "notice-card-views-count-threshold";
constexpr char kNoticeCardClicksCountThresholdParamName[] =
    "notice-card-clicks-count-threshold";

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
  // card to consider it as acknowledged by the user.
  bool HasAcknowledgedNoticeCard() const;

 private:
  bool HasNoticeCardActionsCountPrerequisites(int index);
  void MaybeUpdateNoticeCardViewsCount(int index);
  void MaybeUpdateNoticeCardClicksCount(int index);

  PrefService* profile_prefs_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_NOTICE_CARD_TRACKER_H_