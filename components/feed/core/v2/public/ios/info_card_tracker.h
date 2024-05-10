// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_INFO_CARD_TRACKER_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_INFO_CARD_TRACKER_H_

#import "base/memory/raw_ptr.h"
#import "base/synchronization/lock.h"
#import "base/values.h"
#import "components/feed/core/proto/v2/wire/info_card.pb.h"
#import "components/feed/core/v2/public/ios/prefs.h"

class PrefService;

namespace ios_feed {

namespace {
// This enum represents the types of interactions that are tracked by the
// command.
enum class TrackingType {
  kExplicitDismissal = 0,
  kView = 1,
  kClick = 2,
  kResetState = 3,
  // ReportView forces a view regardless of whether the viewport is adequatly in
  // frame. This is treated the same way as View in this tracker.
  kReportView = 4
};
}  // namespace

// Tracker for the info cards shown in the feeds.
class InfoCardTracker {
 public:
  explicit InfoCardTracker(PrefService* browser_state_prefs);
  ~InfoCardTracker();
  InfoCardTracker(const InfoCardTracker&) = delete;
  InfoCardTracker& operator=(const InfoCardTracker&) = delete;

  // Handles a received TrackInfoCardCommand.
  void OnTrackInfoCardCommand(int info_card_type,
                              int tracking_type,
                              int view_fraction_threshold,
                              int minimum_seconds_between_views);

  // Returns a dict containing all InfoCardTrackingStates.
  const base::Value::Dict& GetInfoCardTrackingStates();

  // Returns the stored tracking state of a given `info_card_type`.
  feedwire::InfoCardTrackingState GetInfoCardTrackingStateFromPref(
      int info_card_type);

 private:
  raw_ptr<PrefService> browser_state_prefs_;

  // Handles a user explicitly dismissing an info card.
  void OnExplicitDismissal(int info_card_type);

  // Handles a user viewing an info card.
  void OnView(int info_card_type, int minimum_seconds_between_views);

  // Handles a user tapping on an info card.
  void OnClick(int info_card_type);

  // Resets the tracking state for a given `info_card_type`.
  void OnResetState(int info_card_type);

  // Creates or updates the tracking state for an info card.
  void SetInfoCardTrackingStateToPref(
      const feedwire::InfoCardTrackingState& tracking_state);

  // Returns the histogram name for logging a given tracking type.
  static std::string GetHistogramForTrackingType(TrackingType tracking_type);
};

}  // namespace ios_feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_INFO_CARD_TRACKER_H_
