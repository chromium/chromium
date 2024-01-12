// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/info_card_tracker.h"

#include <algorithm>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace feed {

using feedwire::InfoCardTrackingState;

namespace {

std::string InfoCardTypeToString(int info_card_type) {
  return base::NumberToString(info_card_type);
}

bool compareInfoCardTrackingState(const InfoCardTrackingState& i1,
                                  const InfoCardTrackingState& i2) {
  return (i1.type() < i2.type());
}

InfoCardTrackingState DecodeFromBase64SerializedString(
    const std::string& base64_serialized_state) {
  InfoCardTrackingState state;

  std::string serialized_state;
  if (!base::Base64Decode(base64_serialized_state, &serialized_state)) {
    DLOG(ERROR) << "Error decoding persisted state from base64";
    return state;
  }

  if (!state.ParseFromString(serialized_state))
    DLOG(ERROR) << "Error parsing InfoCardTrackingState message";

  return state;
}

int64_t GetAdjustedViewTimestamp(int64_t view_timestamp,
                                 int64_t server_timestamp,
                                 int64_t timestamp_adjustment) {
  view_timestamp += timestamp_adjustment;
  // Ensure that the view timestamp does not get earlier than the server.
  if (view_timestamp < server_timestamp)
    view_timestamp = server_timestamp;
  // Ensure that the view timestamp does not exceed the lifetime of the content.
  int64_t max_timestamp =
      server_timestamp +
      GetFeedConfig().content_expiration_threshold.InMilliseconds();
  if (view_timestamp > max_timestamp)
    view_timestamp = max_timestamp;
  return view_timestamp;
}

}  // namespace

InfoCardTracker::InfoCardTracker(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
}

InfoCardTracker::~InfoCardTracker() = default;

std::vector<InfoCardTrackingState> InfoCardTracker::GetAllStates(
    int64_t server_timestamp,
    int64_t client_timestamp) const {
  std::vector<InfoCardTrackingState> states;
  const base::Value& dict = profile_prefs_->GetValue(prefs::kInfoCardStates);
  if (dict.is_dict()) {
    int64_t timestamp_adjustment = server_timestamp - client_timestamp;
    for (const auto pair : dict.GetDict()) {
      int info_card_type = 0;
      if (!base::StringToInt(pair.first, &info_card_type))
        continue;
      if (!pair.second.is_string())
        continue;
      InfoCardTrackingState state =
          DecodeFromBase64SerializedString(pair.second.GetString());
      state.set_type(info_card_type);
      if (state.has_first_view_timestamp()) {
        state.set_first_view_timestamp(
            GetAdjustedViewTimestamp(state.first_view_timestamp(),
                                     server_timestamp, timestamp_adjustment));
      }
      if (state.has_last_view_timestamp()) {
        state.set_last_view_timestamp(
            GetAdjustedViewTimestamp(state.last_view_timestamp(),
                                     server_timestamp, timestamp_adjustment));
      }
      states.push_back(state);
    }
  }
  std::sort(states.begin(), states.end(), compareInfoCardTrackingState);
  return states;
}

void InfoCardTracker::OnViewed(int info_card_type,
                               int minimum_view_interval_seconds) {
  auto now = base::Time::Now();
  InfoCardTrackingState state = GetState(info_card_type);
  if (state.has_last_view_timestamp() &&
      now - feedstore::FromTimestampMillis(state.last_view_timestamp()) <
          base::Seconds(minimum_view_interval_seconds)) {
    return;
  }

  state.set_view_count(state.view_count() + 1);
  if (!state.has_first_view_timestamp())
    state.set_first_view_timestamp(feedstore::ToTimestampMillis(now));
  state.set_last_view_timestamp(feedstore::ToTimestampMillis(now));
  SetState(info_card_type, state);
}

void InfoCardTracker::OnClicked(int info_card_type) {
  InfoCardTrackingState state = GetState(info_card_type);
  state.set_click_count(state.click_count() + 1);
  SetState(info_card_type, state);
}

void InfoCardTracker::OnDismissed(int info_card_type) {
  InfoCardTrackingState state = GetState(info_card_type);
  state.set_explicitly_dismissed_count(state.explicitly_dismissed_count() + 1);
  SetState(info_card_type, state);
}

void InfoCardTracker::ResetState(int info_card_type) {
  InfoCardTrackingState state;
  SetState(info_card_type, state);
}

InfoCardTrackingState InfoCardTracker::GetState(int info_card_type) const {
  const base::Value::Dict& all_states =
      profile_prefs_->GetDict(prefs::kInfoCardStates);
  const std::string* base64_serialized_state =
      all_states.FindString(InfoCardTypeToString(info_card_type));
  if (!base64_serialized_state)
    return InfoCardTrackingState();
  return DecodeFromBase64SerializedString(*base64_serialized_state);
}

void InfoCardTracker::SetState(int info_card_type,
                               const InfoCardTrackingState& state) {
  // SerializeToString encodes the proto into a series of bytes that is not
  // going to be compatible with UTF-8 encoding. We need to convert them to
  // base64 before writing to the prefs store.
  std::string serialized_state;
  state.SerializeToString(&serialized_state);

  const base::Value::Dict& states =
      profile_prefs_->GetDict(prefs::kInfoCardStates);
  base::Value::Dict updated_states = states.Clone();
  updated_states.Set(InfoCardTypeToString(info_card_type),
                     base::Base64Encode(serialized_state));
  profile_prefs_->SetDict(prefs::kInfoCardStates, std::move(updated_states));
}

}  // namespace feed
