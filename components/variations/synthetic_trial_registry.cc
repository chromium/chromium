// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trial_registry.h"

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/variations_associated_data.h"

namespace variations {
namespace internal {

BASE_FEATURE(kExternalExperimentAllowlist,
             "ExternalExperimentAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace internal

SyntheticTrialRegistry::SyntheticTrialRegistry(
    bool enable_external_experiment_allowlist)
    : enable_external_experiment_allowlist_(
          enable_external_experiment_allowlist &&
          base::FeatureList::IsEnabled(
              internal::kExternalExperimentAllowlist)) {}

SyntheticTrialRegistry::SyntheticTrialRegistry()
    : enable_external_experiment_allowlist_(base::FeatureList::IsEnabled(
          internal::kExternalExperimentAllowlist)) {}
SyntheticTrialRegistry::~SyntheticTrialRegistry() = default;

void SyntheticTrialRegistry::AddSyntheticTrialObserver(
    SyntheticTrialObserver* observer) {
  synthetic_trial_observer_list_.AddObserver(observer);
  if (!synthetic_trial_groups_.empty())
    observer->OnSyntheticTrialsChanged(synthetic_trial_groups_);
}

void SyntheticTrialRegistry::RemoveSyntheticTrialObserver(
    SyntheticTrialObserver* observer) {
  synthetic_trial_observer_list_.RemoveObserver(observer);
}

void SyntheticTrialRegistry::RegisterExternalExperiments(
    const std::string& fallback_study_name,
    const std::vector<int>& experiment_ids,
    SyntheticTrialRegistry::OverrideMode mode) {
  DCHECK(!fallback_study_name.empty());

  base::FieldTrialParams params;
  if (enable_external_experiment_allowlist_ &&
      !GetFieldTrialParamsByFeature(internal::kExternalExperimentAllowlist,
                                    &params)) {
    return;
  }

  // When overriding previous external experiments, remove them now.
  if (mode == kOverrideExistingIds) {
    auto is_external = [](const SyntheticTrialGroup& group) {
      return group.is_external();
    };
    base::EraseIf(synthetic_trial_groups_, is_external);
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();
  int trials_added = 0;
  for (int experiment_id : experiment_ids) {
    const std::string experiment_id_str = base::NumberToString(experiment_id);
    const base::StringPiece study_name =
        GetStudyNameForExpId(fallback_study_name, params, experiment_id_str);
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
    trials_added++;
  }

  base::UmaHistogramCounts100("UMA.ExternalExperiment.GroupCount",
                              trials_added);

  if (trials_added > 0)
    NotifySyntheticTrialObservers();
}

void SyntheticTrialRegistry::RegisterSyntheticFieldTrial(
    const SyntheticTrialGroup& trial) {
  for (auto& entry : synthetic_trial_groups_) {
    if (entry.id().name == trial.id().name) {
      // Don't necessarily need to notify observers when setting
      // |annotation_mode| as it is only used when producing metrics reports
      // and does not affect variations service.
      entry.SetAnnotationMode(trial.annotation_mode());
      if (entry.id().group != trial.id().group) {
        entry.SetGroupName(trial.group_name());
        entry.SetStartTime(base::TimeTicks::Now());
        NotifySyntheticTrialObservers();
      }
      return;
    }
  }

  SyntheticTrialGroup trial_group = trial;
  trial_group.SetStartTime(base::TimeTicks::Now());
  synthetic_trial_groups_.push_back(trial_group);
  NotifySyntheticTrialObservers();
}

base::StringPiece SyntheticTrialRegistry::GetStudyNameForExpId(
    const std::string& fallback_study_name,
    const base::FieldTrialParams& params,
    const std::string& experiment_id) {
  if (!enable_external_experiment_allowlist_)
    return fallback_study_name;

  const auto it = params.find(experiment_id);
  if (it == params.end())
    return base::StringPiece();

  // To support additional parameters being passed, besides the study name,
  // truncate the study name at the first ',' character.
  // For example, for an entry like {"1234": "StudyName,FOO"}, we only want the
  // "StudyName" part. This allows adding support for additional things like FOO
  // in the future without backwards compatibility problems.
  const size_t comma_pos = it->second.find(',');
  const size_t truncate_pos =
      (comma_pos == std::string::npos ? it->second.length() : comma_pos);
  return base::StringPiece(it->second.data(), truncate_pos);
}

void SyntheticTrialRegistry::NotifySyntheticTrialObservers() {
  for (SyntheticTrialObserver& observer : synthetic_trial_observer_list_) {
    observer.OnSyntheticTrialsChanged(synthetic_trial_groups_);
  }
}

void SyntheticTrialRegistry::GetSyntheticFieldTrialsOlderThan(
    base::TimeTicks time,
    std::vector<ActiveGroupId>* synthetic_trials,
    base::StringPiece suffix) const {
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
