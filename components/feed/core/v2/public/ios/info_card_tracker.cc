// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/feed/core/v2/public/ios/info_card_tracker.h"

#include <dispatch/dispatch.h>

#import "base/base64.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "components/feed/core/common/pref_names.h"
#import "components/feed/feed_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"

namespace ios_feed {

// Strings for building histogram names for info card tracking.
constexpr char kInfoCardTrackingHistogramName[] =
    "ContentSuggestions.Feed.InfoCard";
constexpr char kInfoCardTrackingHistogramBucketViewed[] = ".Viewed";
constexpr char kInfoCardTrackingHistogramBucketClicked[] = ".Clicked";
constexpr char kInfoCardTrackingHistogramBucketDismissed[] = ".Dismissed";
constexpr char kInfoCardTrackingHistogramBucketReset[] = ".Reset";

InfoCardTracker::InfoCardTracker(PrefService* browser_state_prefs)
    : browser_state_prefs_(browser_state_prefs) {
  CHECK(browser_state_prefs_);
}

InfoCardTracker::~InfoCardTracker() = default;

void InfoCardTracker::OnTrackInfoCardCommand(
    int info_card_type,
    int tracking_type,
    int view_fraction_threshold,
    int minimum_seconds_between_views) {
  TrackingType tracking_type_enum = static_cast<TrackingType>(tracking_type);
  base::UmaHistogramSparse(GetHistogramForTrackingType(tracking_type_enum),
                           info_card_type);
  switch (tracking_type_enum) {
    case TrackingType::kExplicitDismissal:
      OnExplicitDismissal(info_card_type);
      break;
    case TrackingType::kView:
    case TrackingType::kReportView:
      OnView(info_card_type, minimum_seconds_between_views);
      break;
    case TrackingType::kClick:
      OnClick(info_card_type);
      break;
    case TrackingType::kResetState:
      OnResetState(info_card_type);
      break;
    default:
      break;
  }
}

const base::Value::Dict& InfoCardTracker::GetInfoCardTrackingStates() {
  const base::Value::Dict& tracking_state_dict =
      browser_state_prefs_->GetDict(feed::prefs::kInfoCardTrackingStateDict);
  return tracking_state_dict;
}

feedwire::InfoCardTrackingState
InfoCardTracker::GetInfoCardTrackingStateFromPref(int info_card_type) {
  feedwire::InfoCardTrackingState state;
  const base::Value::Dict& tracking_state_dict =
      browser_state_prefs_->GetDict(feed::prefs::kInfoCardTrackingStateDict);
  const std::string* state_string =
      tracking_state_dict.FindString(base::NumberToString(info_card_type));
  if (!state_string) {
    state = feedwire::InfoCardTrackingState();
    state.set_type(info_card_type);
  } else {
    std::string decoded_state;
    if (base::Base64Decode(*state_string, &decoded_state)) {
      const bool success = state.ParseFromString(decoded_state);
      CHECK(success) << "Cannot parse InfoCardTrackingState from pref.";
    }
  }
  return state;
}

void InfoCardTracker::OnExplicitDismissal(int info_card_type) {
  feedwire::InfoCardTrackingState state =
      GetInfoCardTrackingStateFromPref(info_card_type);

  // Increment explicit dismissal count and update stored pref for this tracking
  // state.
  state.set_explicitly_dismissed_count(state.explicitly_dismissed_count() + 1);
  SetInfoCardTrackingStateToPref(state);
}

void InfoCardTracker::OnView(int info_card_type,
                             int minimum_seconds_between_views) {
  feedwire::InfoCardTrackingState state =
      GetInfoCardTrackingStateFromPref(info_card_type);
  int current_timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  // If the last view was logged less than `minimum_seconds_between_views`
  // seconds ago, ignore this view.
  if (current_timestamp - state.last_view_timestamp() <
      minimum_seconds_between_views) {
    return;
  }

  // First time logging a view; update the first view timestamp.
  if (state.first_view_timestamp() == 0) {
    state.set_first_view_timestamp(current_timestamp);
  }

  // Update the last view timestamp and increment the view count.
  state.set_last_view_timestamp(current_timestamp);
  state.set_view_count(state.view_count() + 1);

  // Update the stored pref for this tracking state.
  SetInfoCardTrackingStateToPref(state);
}

void InfoCardTracker::OnClick(int info_card_type) {
  feedwire::InfoCardTrackingState state =
      GetInfoCardTrackingStateFromPref(info_card_type);

  // Increment click count and update stored pref for this tracking state.
  state.set_click_count(state.click_count() + 1);
  SetInfoCardTrackingStateToPref(state);
}

void InfoCardTracker::OnResetState(int info_card_type) {
  // Create a new tracking state and update the stored pref for this
  // `info_card_type` with it.
  feedwire::InfoCardTrackingState state = feedwire::InfoCardTrackingState();
  state.set_type(info_card_type);
  SetInfoCardTrackingStateToPref(state);
}

void InfoCardTracker::SetInfoCardTrackingStateToPref(
    const feedwire::InfoCardTrackingState& tracking_state) {
  int tracking_state_type = tracking_state.type();
  std::string bytes;
  const bool success = tracking_state.SerializeToString(&bytes);
  CHECK(success) << "Cannot serialize InfoCardTrackingState to pref.";
  ScopedDictPrefUpdate update(browser_state_prefs_,
                              feed::prefs::kInfoCardTrackingStateDict);
  update->Set(base::NumberToString(tracking_state_type),
              base::Base64Encode(bytes));
}

// static
std::string InfoCardTracker::GetHistogramForTrackingType(
    TrackingType tracking_type) {
  std::string histogram_prefix(kInfoCardTrackingHistogramName);
  switch (tracking_type) {
    case TrackingType::kExplicitDismissal:
      return histogram_prefix + kInfoCardTrackingHistogramBucketDismissed;
    case TrackingType::kView:
    case TrackingType::kReportView:
      return histogram_prefix + kInfoCardTrackingHistogramBucketViewed;
    case TrackingType::kClick:
      return histogram_prefix + kInfoCardTrackingHistogramBucketClicked;
    case TrackingType::kResetState:
      return histogram_prefix + kInfoCardTrackingHistogramBucketReset;
  }
  NOTREACHED_IN_MIGRATION() << "Tracking type is not supported.";
  return nullptr;
}

}  // namespace ios_feed
