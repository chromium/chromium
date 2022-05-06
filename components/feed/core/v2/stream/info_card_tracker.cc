// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/info_card_tracker.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/config.h"
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

}  // namespace

InfoCardTracker::InfoCardTracker(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
}

InfoCardTracker::~InfoCardTracker() = default;

std::vector<InfoCardTrackingState> InfoCardTracker::GetAllStates() const {
  std::vector<InfoCardTrackingState> states;
  const base::Value* dict = profile_prefs_->Get(prefs::kInfoCardStates);
  if (dict && dict->is_dict()) {
    for (const auto pair : dict->DictItems()) {
      int info_card_type = 0;
      if (!base::StringToInt(pair.first, &info_card_type))
        continue;
      if (!pair.second.is_string())
        continue;
      InfoCardTrackingState state;
      state.ParseFromString(pair.second.GetString());
      state.set_type(info_card_type);
      states.push_back(state);
    }
  }
  std::sort(states.begin(), states.end(), compareInfoCardTrackingState);
  return states;
}

void InfoCardTracker::OnViewed(int info_card_type,
                               int minimum_view_interval_seconds) {
  auto now = base::TimeTicks::Now();
  auto iter = last_view_times_.find(info_card_type);
  if (iter != last_view_times_.end() &&
      now - iter->second < base::Seconds(minimum_view_interval_seconds)) {
    return;
  }
  last_view_times_[info_card_type] = now;

  InfoCardTrackingState state = GetState(info_card_type);
  state.set_view_count(state.view_count() + 1);
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
  InfoCardTrackingState state;
  const base::Value* all_states =
      profile_prefs_->GetDictionary(prefs::kInfoCardStates);
  if (all_states) {
    const std::string* serialized_state =
        all_states->FindStringKey(InfoCardTypeToString(info_card_type));
    if (serialized_state) {
      if (!state.ParseFromString(*serialized_state))
        DLOG(ERROR) << "Error parsing InfoCardTrackingState message";
    }
  }
  return state;
}

void InfoCardTracker::SetState(int info_card_type,
                               const InfoCardTrackingState& state) {
  std::string serialized_state;
  state.SerializeToString(&serialized_state);

  base::Value updated_states(base::Value::Type::DICTIONARY);
  const base::Value* states = profile_prefs_->Get(prefs::kInfoCardStates);
  if (states && states->is_dict()) {
    updated_states = states->Clone();
  }
  updated_states.SetStringKey(InfoCardTypeToString(info_card_type),
                              serialized_state);
  profile_prefs_->Set(prefs::kInfoCardStates, updated_states);
}

}  // namespace feed
