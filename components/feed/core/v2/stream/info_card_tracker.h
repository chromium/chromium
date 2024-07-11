// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_INFO_CARD_TRACKER_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_INFO_CARD_TRACKER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/info_card.pb.h"
#include "components/feed/core/v2/public/common_enums.h"

class PrefService;

namespace feed {

// Tracks the raw counts of the info cards in order to determine their
// acknowledgement states.
class InfoCardTracker {
 public:
  explicit InfoCardTracker(PrefService* profile_prefs);
  ~InfoCardTracker();

  InfoCardTracker(const InfoCardTracker&) = delete;
  InfoCardTracker& operator=(const InfoCardTracker&) = delete;

  // Returns the list of states of all tracked info cards. The returned view
  // timestamps will be adjusted to be based on server's clock. The adjustment
  // is computed based on `server_timestamp` and `client_timestamp`.
  // `server_timestamp` is the server timestamp, in milliseconds from Epoch,
  // when the response is produced.
  // `client_timestamp` is the client timestamp, in milliseconds from Epoch,
  // when the response is received.
  std::vector<feedwire::InfoCardTrackingState> GetAllStates(
      int64_t server_timestamp,
      int64_t client_timestamp) const;

  // Called when the info card is fully visible.
  void OnViewed(int info_card_type, int minimum_view_interval_seconds);

  // Called when the info card is tapped.
  void OnClicked(int info_card_type);

  // Called when the info card is dismissed explicitly.
  void OnDismissed(int info_card_type);

  // Reset the state of the info card.
  void ResetState(int info_card_type);

 private:
  feedwire::InfoCardTrackingState GetState(int info_card_type) const;
  void SetState(int info_card_type,
                const feedwire::InfoCardTrackingState& state);

  raw_ptr<PrefService, DanglingUntriaged> profile_prefs_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_INFO_CARD_TRACKER_H_
