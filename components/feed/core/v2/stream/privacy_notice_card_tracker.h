// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_PRIVACY_NOTICE_CARD_TRACKER_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_PRIVACY_NOTICE_CARD_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class PrefService;

namespace feedwire {
class ContentId;
}
namespace feed {

// Tracker for the notice card related actions that also provide signals based
// on those.
class PrivacyNoticeCardTracker {
 public:
  explicit PrivacyNoticeCardTracker(PrefService* profile_prefs);

  PrivacyNoticeCardTracker(const PrivacyNoticeCardTracker&) = delete;
  PrivacyNoticeCardTracker& operator=(const PrivacyNoticeCardTracker&) = delete;

  // Capture the actions.

  // Should be called whenever a card has been viewed.
  void OnCardViewed(bool is_signed_in, const feedwire::ContentId& content_id);
  // Should be called whenever a card on the Feed is tapped.
  void OnOpenAction(const feedwire::ContentId& content_id);

  // Get signals based on the actions.

  // Indicates whether there were enough views or clicks done on the notice
  // card to consider it as acknowledged by the user.
  bool HasAcknowledgedNoticeCard() const;

 private:
  void MaybeUpdateNoticeCardClicksCount(int index);

  raw_ptr<PrefService> profile_prefs_;

  // The number of views of the notice card.
  int views_count_;

  // The number of clicks/taps of the notice card.
  bool has_clicked_;

  // Whether there was previously a notice card view reported this session.
  base::TimeTicks last_view_time_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_PRIVACY_NOTICE_CARD_TRACKER_H_
