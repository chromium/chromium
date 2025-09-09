// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/fre_source_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/metrics_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace metrics::fre_source_trial {
namespace {

scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider) {
  return base::FieldTrialList::FactoryGetFieldTrial(
      kFRESourceTrial, /*total_probability=*/100, kDefaultGroup,
      entropy_provider);
}

std::string CreateFirstRunTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    version_info::Channel channel) {
  int enabled_percent = 0;
  int control_percent = 0;
  int default_percent = 100;
  switch (channel) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      enabled_percent = 50;
      control_percent = 50;
      default_percent = 0;
      break;
    case version_info::Channel::STABLE:
      enabled_percent = 1;
      control_percent = 1;
      default_percent = 98;
      break;
    default:
      break;
  }

  // Set up the trial and groups.
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial(entropy_provider);
  trial->AppendGroup(kEnabledGroup, enabled_percent);
  trial->AppendGroup(kControlGroup, control_percent);
  trial->AppendGroup(kDefaultGroup, default_percent);

  // Finalize the group choice. `group_name()` calls `Activate()` internally.
  return trial->group_name();
}

void CreateSubsequentRunTrial(
    const std::string& group_name,
    const base::FieldTrial::EntropyProvider& entropy_provider) {
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial(entropy_provider);
  trial->AppendGroup(group_name, /*group_probability=*/100);
  trial->Activate();
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kFRESourceTrial, "");
}

bool IsEnabled() {
  return base::FieldTrialList::FindFullName(kFRESourceTrial) == kEnabledGroup;
}

void Create(PrefService* local_state,
            const base::FieldTrial::EntropyProvider& entropy_provider,
            version_info::Channel channel,
            bool is_fre) {
  CHECK(!base::FieldTrialList::TrialExists(kFRESourceTrial))
      << "Trial already exists.";
  const std::string& trial_group = local_state->GetString(kFRESourceTrial);
  if (is_fre && trial_group.empty()) {
    const std::string new_trial_group =
        CreateFirstRunTrial(entropy_provider, channel);
    // Persist the assigned group for subsequent runs.
    local_state->SetString(kFRESourceTrial, new_trial_group);
  } else if (!trial_group.empty()) {
    // Group already assigned in a previous run, so we assign it again.
    CreateSubsequentRunTrial(trial_group, entropy_provider);
  }
}

}  // namespace metrics::fre_source_trial
