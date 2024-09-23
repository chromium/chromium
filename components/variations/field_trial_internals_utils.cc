// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_internals_utils.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/version.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/hashing.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_layers.h"
#include "components/version_info/version_info.h"

namespace variations {
namespace {
using TrialGroup = std::pair<std::string, std::string>;
using variations::HashNameAsHexString;

// Returns whether (`study_name`, `experiment_name`) is found in `studies`.
bool ContainsExperiment(const std::vector<variations::StudyGroupNames>& studies,
                        std::string_view study_name,
                        std::string_view experiment_name) {
  for (const auto& study : studies) {
    if (study.name == study_name) {
      if (base::Contains(study.groups, experiment_name)) {
        return true;
      }
    }
  }
  return false;
}

// Expiration state for field trial overrides. Note that all overrides share the
// same expiration.
struct ExpirationInfo {
  // Are field trial overrides expired, either by number of restarts or by
  // elapsed time.
  bool expired = false;
  // Number of Chrome restarts.
  int chrome_start_count = 0;
};

ExpirationInfo GetExpirationInfo(PrefService& local_state) {
  ExpirationInfo result{.expired = true, .chrome_start_count = 0};
  base::Time expiration =
      local_state.GetTime(variations::prefs::kVariationsForcedTrialExpiration);
  if (expiration.is_null()) {
    return result;
  }

  result.chrome_start_count =
      local_state.GetInteger(variations::prefs::kVariationsForcedTrialStarts);
  base::TimeDelta remaining_time = expiration - base::Time::Now();

  // Is it expired by time? If the expiry time too far in the future, treat
  // it as expired as well.
  if (remaining_time < base::TimeDelta() ||
      remaining_time > kManualForceFieldTrialDuration ||
      result.chrome_start_count >=
          kChromeStartCountBeforeResetForcedFieldTrials) {
    // Clear the time pref, so that GetExpirationInfo can read only one pref
    // next time.
    local_state.ClearPref(variations::prefs::kVariationsForcedTrialExpiration);

    return result;
  }

  // Not expired by time or by restart count.
  result.expired = false;
  return result;
}

}  // namespace

StudyGroupNames::StudyGroupNames(const Study& study) {
  name = study.name();
  for (const auto& group : study.experiment()) {
    groups.push_back(group.name());
  }
}

StudyGroupNames::StudyGroupNames() = default;
StudyGroupNames::~StudyGroupNames() = default;
StudyGroupNames::StudyGroupNames(const StudyGroupNames& src) = default;
StudyGroupNames& StudyGroupNames::operator=(const StudyGroupNames& src) =
    default;

std::vector<StudyGroupNames> GetStudiesAvailableToForce(
    VariationsSeed seed,
    const EntropyProviders& entropy_providers,
    const ClientFilterableState& client_filterable_state) {
  std::vector<StudyGroupNames> result;
  const base::Version& current_version = version_info::GetVersion();
  if (!current_version.IsValid()) {
    return result;
  }

  VariationsLayers layers(seed, entropy_providers);
  std::vector<variations::ProcessedStudy> filtered_studies =
      FilterAndValidateStudies(seed, client_filterable_state, layers);

  for (const auto& processed_study : filtered_studies) {
    // Remove groups that are forced by flags or features.
    Study study = *processed_study.study();
    auto exp = study.mutable_experiment()->begin();
    while (exp != study.mutable_experiment()->end()) {
      if (exp->has_forcing_flag() ||
          exp->feature_association().has_forcing_feature_off() ||
          exp->feature_association().has_forcing_feature_on()) {
        exp = study.mutable_experiment()->erase(exp);
      } else {
        ++exp;
      }
    }
    if (study.experiment_size() > 0) {
      result.emplace_back(study);
    }
  }
  return result;
}

void RegisterFieldTrialInternalsPrefs(PrefRegistrySimple& registry) {
  registry.RegisterStringPref(prefs::kVariationsForcedFieldTrials,
                              std::string());
  registry.RegisterTimePref(prefs::kVariationsForcedTrialExpiration,
                            base::Time());
  registry.RegisterIntegerPref(prefs::kVariationsForcedTrialStarts, 0);
}

void ForceTrialsAtStartup(PrefService& local_state) {
  ExpirationInfo expiration = GetExpirationInfo(local_state);
  base::UmaHistogramBoolean(
      "Variations.ForcedFieldTrialsAtStartupForInternalsPage",
      !expiration.expired);
  if (expiration.expired) {
    return;
  }

  local_state.SetInteger(variations::prefs::kVariationsForcedTrialStarts,
                         expiration.chrome_start_count + 1);
  // Write eagerly to avoid a crash loop.
  local_state.CommitPendingWrite();

  std::string forced_field_trials =
      local_state.GetString(variations::prefs::kVariationsForcedFieldTrials);
  bool result = base::FieldTrialList::CreateTrialsFromString(
      forced_field_trials, /*override_trials=*/true);
  if (!result) {
    DLOG(WARNING) << "Failed to create field trials from "
                     "MetricsInternalsForcedFieldTrials: "
                  << forced_field_trials;
  }
}

bool SetTemporaryTrialOverrides(
    PrefService& local_state,
    base::span<std::pair<std::string, std::string>> override_groups) {
  std::vector<base::FieldTrial::State> states;
  for (const auto& g : override_groups) {
    base::FieldTrial::State state;
    state.trial_name = g.first;
    state.group_name = g.second;
    states.push_back(std::move(state));
  }
  std::string force_string =
      base::FieldTrial::BuildFieldTrialStateString(states);

  std::string prior_force_string =
      local_state.GetString(variations::prefs::kVariationsForcedFieldTrials);
  ExpirationInfo expiration_info = GetExpirationInfo(local_state);
  bool needs_restart = prior_force_string != force_string;

  // Update the cached ForcedFieldTrials with the new state
  local_state.SetString(variations::prefs::kVariationsForcedFieldTrials,
                        force_string);
  if (force_string.empty()) {
    local_state.SetTime(variations::prefs::kVariationsForcedTrialExpiration,
                        base::Time());
    return needs_restart;
  }

  needs_restart = needs_restart || expiration_info.expired ||
                  expiration_info.chrome_start_count == 0;
  base::Time expiration;
  if (!override_groups.empty()) {
    expiration = base::Time::Now() + kManualForceFieldTrialDuration;
  }
  local_state.SetTime(variations::prefs::kVariationsForcedTrialExpiration,
                      expiration);
  // If restart is not required, set number of starts to 1 instead of 0. This
  // is a way to remember that restart is required, as after restart this
  // value will be incremented.
  local_state.SetInteger(variations::prefs::kVariationsForcedTrialStarts,
                         needs_restart ? 0 : 1);

  return needs_restart;
}

base::flat_map<std::string, std::string> RefreshAndGetFieldTrialOverrides(
    const std::vector<variations::StudyGroupNames>& available_studies,
    PrefService& local_state,
    bool& requires_restart) {
  requires_restart = false;
  ExpirationInfo expiration = GetExpirationInfo(local_state);
  if (expiration.expired) {
    return {};
  }

  base::flat_map<std::string, std::string> overrides;
  std::vector<base::FieldTrial::State> entries;
  base::FieldTrial::ParseFieldTrialsString(
      local_state.GetString(variations::prefs::kVariationsForcedFieldTrials),
      /*override_trials=*/true, entries);

  std::vector<std::pair<std::string, std::string>> active_groups;
  for (const base::FieldTrial::State& state : entries) {
    if (ContainsExperiment(available_studies, state.trial_name,
                           state.group_name)) {
      overrides[HashNameAsHexString(state.trial_name)] =
          HashNameAsHexString(state.group_name);
      active_groups.emplace_back(state.trial_name, state.group_name);
    }
  }
  requires_restart = SetTemporaryTrialOverrides(local_state, active_groups);
  return overrides;
}

}  // namespace variations
