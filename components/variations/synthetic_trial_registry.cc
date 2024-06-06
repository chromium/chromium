// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trial_registry.h"

#include "base/containers/contains.h"
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
BASE_FEATURE(kExternalExperimentAllowlist,
             "ExternalExperimentAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
    const std::string_view study_name =
        GetStudyNameForExpId(params, experiment_id_str);
    if (study_name.empty())
      continue;

    const uint32_t trial_hash = HashName(study_name);
    // If existing ids shouldn't be overridden, skip entries whose study names
    // are already registered.
    if (mode == kDoNotOverrideExistingIds) {
      if (base::Contains(synthetic_trial_groups_, trial_hash,
                         [](const SyntheticTrialGroup& group) {
                           return group.id().name;
                         })) {
        continue;
      }
    }

    const uint32_t group_hash = HashName(experiment_id_str);

    // Since external experiments are not based on Chrome's low entropy source,
    // they are only sent to Google web properties for signed-in users to make
    // sure they couldn't be used to identify a user that's not signed-in.
    AssociateGoogleVariationIDForceHashes(
        GOOGLE_WEB_PROPERTIES_SIGNED_IN, {trial_hash, group_hash},
        static_cast<VariationID>(experiment_id));
    SyntheticTrialGroup entry(
        study_name, experiment_id_str,
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

std::string_view SyntheticTrialRegistry::GetStudyNameForExpId(
    const base::FieldTrialParams& params,
    const std::string& experiment_id) {
  const auto it = params.find(experiment_id);
  if (it == params.end()) {
    return std::string_view();
  }

  // To support additional parameters being passed, besides the study name,
  // truncate the study name at the first ',' character.
  // For example, for an entry like {"1234": "StudyName,FOO"}, we only want the
  // "StudyName" part. This allows adding support for additional things like FOO
  // in the future without backwards compatibility problems.
  const size_t comma_pos = it->second.find(',');
  const size_t truncate_pos =
      (comma_pos == std::string::npos ? it->second.length() : comma_pos);
  return std::string_view(it->second.data(), truncate_pos);
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
        entry.annotation_mode() == SyntheticTrialAnnotationMode::kCurrentLog)
      active_groups.push_back(entry.active_group());
  }

  GetFieldTrialActiveGroupIdsForActiveGroups(suffix, active_groups,
                                             synthetic_trials);
}

}  // namespace variations
