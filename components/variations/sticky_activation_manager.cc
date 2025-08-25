// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/sticky_activation_manager.h"

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/strings/string_split.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace variations {
namespace {

// Used as the group names for studies that we know have STICKY_AFTER_QUERY
// activation, but haven't been made active yet.
//
// Note: We intentionally use the same character as the separator for the pref,
// since a) that character is already reserved and can't appear naturally in
// these strings and b) to guarantee it's not something we'd load or save to the
// pref, as doing so would make it invalid.
const char kInactiveStickyTrialSentinel[] = "/";

// Parses the sticky studies pref value, which is expected to be of the format
// "Study1/Group1/Study2/Group2" and returns as a map from trial names to
// groups names.
StickyActivationManager::TrialNameToGroupNameMap ParsePref(
    const std::string& pref_value) {
  StickyActivationManager::TrialNameToGroupNameMap result;

  // Note: Even though base::FieldTrial::ParseFieldTrialsString() provides more
  // features than we need, by using it we benefit from the validation it does.
  std::vector<base::FieldTrial::State> entries;
  if (!base::FieldTrial::ParseFieldTrialsString(
          pref_value, /*override_trials=*/false, entries)) {
    // This is not a CHECK() since the pref value is external, but we still want
    // to monitor the occurrence of invalid prefs in case there is a a code
    // issue, so dump without crashing to signal the issue.
    base::debug::DumpWithoutCrashing();
    return result;
  }
  for (const auto& entry : entries) {
    result[std::string(entry.trial_name)] = std::string(entry.group_name);
  }
  return result;
}

// Encodes `trials` as a string pref value of the format
// "Study1/Group1/Study2/Group2".
std::string EncodePref(
    const StickyActivationManager::TrialNameToGroupNameMap& trials) {
  std::string pref_value;
  for (const auto& [key, value] : trials) {
    if (value == kInactiveStickyTrialSentinel) {
      continue;
    }
    if (!pref_value.empty()) {
      base::StrAppend(&pref_value, {"/"});
    }
    base::StrAppend(&pref_value, {key, "/", value});
  }
  return pref_value;
}

}  // namespace

StickyActivationManager::StickyActivationManager(PrefService* local_state,
                                                 bool sticky_activation_enabled)
    : local_state_(local_state),
      sticky_activation_enabled_(sticky_activation_enabled) {
  if (local_state && sticky_activation_enabled_) {
    loaded_sticky_trials_ =
        ParsePref(local_state_->GetString(prefs::kVariationsStickyStudies));
  }
}

StickyActivationManager::~StickyActivationManager() {
  if (monitoring_started_ && sticky_activation_enabled_) {
    base::FieldTrialListIncludingLowAnonymity::RemoveObserver(this);
  }
}

// static
void StickyActivationManager::RegisterPrefs(PrefRegistrySimple& registry) {
  registry.RegisterStringPref(prefs::kVariationsStickyStudies, "",
                              PrefRegistry::LOSSY_PREF);
}

void StickyActivationManager::StartMonitoring() {
  CHECK(!monitoring_started_);
  monitoring_started_ = true;

  if (!sticky_activation_enabled_) {
    return;
  }

  // Clear the loaded sticky trials, since these are no longer needed. The
  // entries that were activated have been copied over to
  // `active_sticky_trials_`.
  loaded_sticky_trials_.clear();

  base::FieldTrialListIncludingLowAnonymity::AddObserver(this);

  UpdatePref();
}

bool StickyActivationManager::ShouldActivate(const std::string& trial_name,
                                             const std::string& group_name) {
  CHECK(!monitoring_started_);
  if (!sticky_activation_enabled_) {
    return false;
  }

  auto it = loaded_sticky_trials_.find(trial_name);
  if (it != loaded_sticky_trials_.end() && it->second == group_name) {
    active_sticky_trials_[trial_name] = group_name;
    return true;
  }
  // Otherwise, we know this is a sticky trial and it's not active yet, so
  // reserve a slot for it so we can tell it's a sticky trial when we observe
  // its activation.
  active_sticky_trials_[trial_name] = kInactiveStickyTrialSentinel;
  return false;
}

void StickyActivationManager::OnFieldTrialGroupFinalized(
    const base::FieldTrial& trial,
    const std::string& group_name) {
  CHECK(monitoring_started_);
  CHECK(sticky_activation_enabled_);

  // Check whether the trial is present in `active_sticky_trials_`, which is how
  // we track which trials have the STICKY_AFTER_QUERY activation type.
  auto it = active_sticky_trials_.find(trial.trial_name());
  if (it != active_sticky_trials_.end()) {
    // We don't expect to be notified of the same trial twice, so the entry for
    // this trial should be the sentinel.
    //
    // Note: We DCHECK() instead of CHECK() here because this code being hit
    // relies both on a client-side coding bug, but also a specific server-side
    // payload that would exercise this (i.e. existence of STICKY_AFTER_QUERY
    // studies). We don't want a case where the client-side bug is introduced
    // but the server-side payload not exercising this to make it to Stable and
    // then start crashing lots of users, so use a DCHECK.
    DCHECK_EQ(it->second, kInactiveStickyTrialSentinel);

    it->second = group_name;
    UpdatePref();
  }
}

void StickyActivationManager::UpdatePref() {
  CHECK(monitoring_started_);
  CHECK(sticky_activation_enabled_);

  // TODO: crbug.com/435630455 - Instead of updating the pref each time,
  // schedule an update so that we can batch multiple updates together.
  if (!local_state_) {
    return;
  }

  std::string pref_value = EncodePref(active_sticky_trials_);
  local_state_->SetString(prefs::kVariationsStickyStudies, pref_value);
}

}  // namespace variations
