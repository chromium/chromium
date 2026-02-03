// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trial_registry.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/variations_associated_data.h"

namespace variations {
namespace internal {

// Used to deliver the allowlist via a feature param. If disabled, the
// allowlist is treated as empty (nothing allowed).
BASE_FEATURE(kExternalExperimentAllowlist, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace internal

SyntheticTrialRegistry::SyntheticTrialRegistry() = default;
SyntheticTrialRegistry::~SyntheticTrialRegistry() = default;

void SyntheticTrialRegistry::AddObserver(SyntheticTrialObserver* observer) {
  synthetic_trial_observer_list_.AddObserver(observer);
  if (!synthetic_trial_groups_.empty()) {
    observer->OnSyntheticTrialsChanged(synthetic_trial_groups_, {},
                                       synthetic_trial_groups_);
  }
}

void SyntheticTrialRegistry::RemoveObserver(SyntheticTrialObserver* observer) {
  synthetic_trial_observer_list_.RemoveObserver(observer);
}

void SyntheticTrialRegistry::RegisterExternalExperiments(
    base::PassKey<UmaSessionStatsExternalExperimentRegistrar> pass_key,
    const std::vector<int>& experiment_ids,
    SyntheticTrialRegistry::OverrideMode mode) {
  RegisterExternalExperimentsInternal(experiment_ids, mode);
}

void SyntheticTrialRegistry::RegisterExternalExperiments(
    base::PassKey<android_webview::AwMetricsServiceAccessor> pass_key,
    const std::vector<int>& experiment_ids,
    SyntheticTrialRegistry::OverrideMode mode) {
  RegisterExternalExperimentsInternal(experiment_ids, mode);
}

void SyntheticTrialRegistry::RegisterExternalExperimentsForTesting(
    const std::vector<int>& experiment_ids,
    SyntheticTrialRegistry::OverrideMode mode) {
  RegisterExternalExperimentsInternal(experiment_ids, mode);
}

std::vector<ActiveGroupId>
SyntheticTrialRegistry::GetCurrentSyntheticFieldTrialsForTest() const {
  CHECK_IS_TEST();
  std::vector<ActiveGroupId> synthetic_trials;
  GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(), &synthetic_trials);
  return synthetic_trials;
}

void SyntheticTrialRegistry::RegisterExternalExperimentsInternal(
    const std::vector<int>& experiment_ids,
    SyntheticTrialRegistry::OverrideMode mode) {
  base::FieldTrialParams params;
  GetFieldTrialParamsByFeature(internal::kExternalExperimentAllowlist, &params);
  // If no params (empty allowlist or feature disabled), no external experiments
  // are allowed.
  if (params.empty()) {
    return;
  }

  std::vector<SyntheticTrialGroup> trials_updated;
  std::vector<SyntheticTrialGroup> trials_removed;

  // When overriding previous external experiments, remove them now.
  if (mode == kOverrideExistingIds) {
    auto it = synthetic_trial_groups_.begin();
    while (it != synthetic_trial_groups_.end()) {
      if (it->is_external()) {
        trials_removed.push_back(*it);
        // Keep iterator valid after erase.
        it = synthetic_trial_groups_.erase(it);
      } else {
        ++it;
      }
    }
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();
  for (int experiment_id : experiment_ids) {
    const std::string experiment_id_str = base::NumberToString(experiment_id);
    const ExternalExperiment experiment =
        GetExternalExperiment(params, experiment_id_str);
    if (experiment.study_name.empty()) {
      continue;
    }

    const uint32_t trial_hash = HashName(experiment.study_name);
    // If existing ids shouldn't be overridden, skip entries whose study names
    // are already registered.
    if (mode == kDoNotOverrideExistingIds) {
      if (std::ranges::contains(synthetic_trial_groups_, trial_hash,
                                [](const SyntheticTrialGroup& group) {
                                  return group.id().name;
                                })) {
        continue;
      }
    }
    const uint32_t group_hash = HashName(experiment.group_name);

    // Since external experiments are not based on Chrome's low or limited
    // entropy sources, they are sent to Google web properties only for
    // signed-in users to make sure they couldn't be used to identify a user
    // that's not signed-in.
    AssociateGoogleVariationID(
        base::PassKey<SyntheticTrialRegistry>(),
        GOOGLE_WEB_PROPERTIES_SIGNED_IN, {trial_hash, group_hash},
        static_cast<VariationID>(experiment_id), variations::TimeWindow());
    SyntheticTrialGroup entry(
        experiment.study_name, experiment.group_name,
        variations::SyntheticTrialAnnotationMode::kNextLog);
    entry.SetStartTime(start_time);
    entry.SetIsExternal(true);
    synthetic_trial_groups_.push_back(entry);
    trials_updated.push_back(entry);
  }

  base::UmaHistogramCounts100("UMA.ExternalExperiment.GroupCount",
                              trials_updated.size());

  if (!trials_updated.empty() || !trials_removed.empty()) {
    NotifySyntheticTrialObservers(trials_updated, trials_removed);
  }
}

void SyntheticTrialRegistry::RegisterSyntheticFieldTrial(
    const SyntheticTrialGroup& trial) {
  for (auto& entry : synthetic_trial_groups_) {
    if (entry.id().name == trial.id().name) {
      if (entry.id().group != trial.id().group ||
          entry.annotation_mode() != trial.annotation_mode()) {
        entry.SetAnnotationMode(trial.annotation_mode());
        entry.SetGroupName(trial.group_name());
        entry.SetStartTime(base::TimeTicks::Now());
        NotifySyntheticTrialObservers({entry}, {});
      }
      return;
    }
  }

  SyntheticTrialGroup trial_group = trial;
  trial_group.SetStartTime(base::TimeTicks::Now());
  synthetic_trial_groups_.push_back(trial_group);
  NotifySyntheticTrialObservers({trial_group}, {});
}

SyntheticTrialRegistry::ExternalExperiment
SyntheticTrialRegistry::GetExternalExperiment(
    const base::FieldTrialParams& params,
    const std::string& experiment_id) {
  const auto it = params.find(experiment_id);
  if (it == params.end()) {
    return SyntheticTrialRegistry::ExternalExperiment();
  }
  const std::string_view full_value(it->second);

  // The config format is "StudyName" or "StudyName,GroupName".
  // The study name is everything before the first comma.
  const size_t first_comma_pos = full_value.find(',');
  const std::string_view study_name(full_value.data(),
                                    first_comma_pos == std::string::npos
                                        ? it->second.length()
                                        : first_comma_pos);

  if (first_comma_pos == std::string::npos) {
    return {study_name, experiment_id};
  }

  // The group name is the part between the first and second comma.
  // If there is a second comma, anything after it is ignored.
  const size_t group_name_start = first_comma_pos + 1;
  const size_t second_comma_pos = full_value.find(',', group_name_start);
  std::string_view group_name;
  if (second_comma_pos == std::string::npos) {
    group_name = full_value.substr(group_name_start);
  } else {
    group_name = full_value.substr(group_name_start,
                                   second_comma_pos - group_name_start);
  }
  return {study_name, group_name.empty() ? experiment_id : group_name};
}

void SyntheticTrialRegistry::NotifySyntheticTrialObservers(
    const std::vector<SyntheticTrialGroup>& trials_updated,
    const std::vector<SyntheticTrialGroup>& trials_removed) {
  for (SyntheticTrialObserver& observer : synthetic_trial_observer_list_) {
    observer.OnSyntheticTrialsChanged(trials_updated, trials_removed,
                                      synthetic_trial_groups_);
  }
}

void SyntheticTrialRegistry::GetSyntheticFieldTrialsOlderThan(
    base::TimeTicks time,
    std::vector<ActiveGroupId>* synthetic_trials,
    std::string_view suffix) const {
  DCHECK(synthetic_trials);
  synthetic_trials->clear();
  base::FieldTrial::ActiveGroups active_groups;
  for (const auto& entry : synthetic_trial_groups_) {
    if (entry.start_time() <= time ||
        entry.annotation_mode() == SyntheticTrialAnnotationMode::kCurrentLog) {
      active_groups.push_back(entry.active_group());
    }
  }

  GetFieldTrialActiveGroupIdsForActiveGroups(suffix, active_groups,
                                             synthetic_trials);
}

}  // namespace variations
