// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_NOTICE_CARD_TRACKER_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_NOTICE_CARD_TRACKER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"

class PrefService;

namespace base {
class Value;
}  // namespace base

namespace feed {

// Tracker for the notice's acknowledgement state.
class NoticeCardTracker {
 public:
  // The number of views of the notice to be considered as acknowledged by the
  // user.
  static const int kViewCountThreshold;
  // The number of clicks of the notice to be considered as acknowledged by the
  // user.
  static const int kClickCountThreshold;

  // Returns all keys of notices that have been acknowledged.
  static std::vector<std::string> GetAllAckowledgedKeys(
      PrefService* profile_prefs);

  NoticeCardTracker(PrefService* profile_prefs, const std::string& key);
  ~NoticeCardTracker();

  NoticeCardTracker(const NoticeCardTracker&) = delete;
  NoticeCardTracker& operator=(const NoticeCardTracker&) = delete;

  // Called when this notice has been viewed.
  void OnViewed();

  // Called when the user has clicked/tapped this notice to perform an open
  // action.
  void OnOpenAction();

  // Called when this notice is dismissed.
  void OnDismissed();

  // Indicates whether the notice is acknowledged by the user. The notice is
  // acknowledged when one of the following conditions is met:
  // 1) There were enough views.
  // 2) Acknowdleged by the user directly when the user clicks or dismisses it.
  bool HasAcknowledged() const;

 private:
  static bool CanBeTreatedAsAcknowledged(int views_count,
                                         int clicks_count,
                                         int dismissed);
  const base::Value* GetStates() const;
  int GetViewCount() const;
  int GetClickCount() const;
  int GetDismissState() const;
  void IncrementViewCount();
  void IncrementClickCount();
  int GetCount(base::StringPiece dict_key) const;
  void SetCount(base::StringPiece, int new_count);

  raw_ptr<PrefService> profile_prefs_;
  std::string key_;
  base::TimeTicks last_view_time_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_NOTICE_CARD_TRACKER_H_
