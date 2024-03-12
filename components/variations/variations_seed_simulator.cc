// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_simulator.h"

#include <stddef.h>

#include <map>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/metrics/field_trial_params.h"
#include "base/types/optional_ref.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"

namespace variations {

namespace {

enum ChangeType {
  NO_CHANGE,
  CHANGED,
  CHANGED_KILL_BEST_EFFORT,
  CHANGED_KILL_CRITICAL,
};

// Simulate group assignment for the specified study with PERMANENT consistency.
// Returns the experiment group that will be selected. Mirrors logic in
// VariationsSeedProcessor::CreateTrialFromStudy().
std::string SimulateGroupAssignment(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    const ProcessedStudy& processed_study) {
  const Study& study = *processed_study.study();
  DCHECK_EQ(Study_Consistency_PERMANENT, study.consistency());

  const double entropy_value =
      entropy_provider.GetEntropyForTrial(study.name(),
                                          study.randomization_seed());
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrial::CreateSimulatedFieldTrial(
          study.name(), processed_study.total_probability(),
          processed_study.GetDefaultExperimentName(), entropy_value));

  for (const auto& experiment : study.experiment()) {
    // TODO(asvitkine): This needs to properly handle the case where a group was
    // forced via forcing_flag in the current state, so that it is not treated
    // as changed.
    if (!experiment.has_forcing_flag() &&
        experiment.name() != study.default_experiment_name()) {
      trial->AppendGroup(experiment.name(), experiment.probability_weight());
    }
  }
  return trial->group_name();
}

// Finds an experiment in |study| with name |experiment_name| and returns it,
// or NULL if it does not exist.
const Study_Experiment* FindExperiment(const Study& study,
                                       const std::string& experiment_name) {
  for (const auto& experiment : study.experiment()) {
    if (experiment.name() == experiment_name)
      return &experiment;
  }
  return nullptr;
}

// Checks whether experiment params set for |experiment| on |study| are exactly
// equal to the params registered for the corresponding field trial in the
// current process.
bool VariationParamsAreEqual(const Study& study,
                             const Study_Experiment& experiment) {
  std::map<std::string, std::string> params;
  base::GetFieldTrialParams(study.name(), &params);

  if (static_cast<int>(params.size()) != experiment.param_size())
    return false;

  for (const auto& param : experiment.param()) {
    std::map<std::string, std::string>::const_iterator it =
        params.find(param.name());
    if (it == params.end() || it->second != param.value())
      return false;
  }

  return true;
}

ChangeType ConvertExperimentTypeToChangeType(Study_Experiment_Type type) {
  switch (type) {
    case Study_Experiment_Type_NORMAL:
      return CHANGED;
    case Study_Experiment_Type_IGNORE_CHANGE:
      return NO_CHANGE;
    case Study_Experiment_Type_KILL_BEST_EFFORT:
      return CHANGED_KILL_BEST_EFFORT;
    case Study_Experiment_Type_KILL_CRITICAL:
      return CHANGED_KILL_CRITICAL;
  }
  return CHANGED;
}

ChangeType PermanentStudyGroupChanged(
    const ProcessedStudy& processed_study,
    const std::string& selected_group,
    const VariationsLayers& layers,
    const base::FieldTrial::EntropyProvider& entropy_provider) {
  const Study& study = *processed_study.study();
  DCHECK_EQ(Study_Consistency_PERMANENT, study.consistency());

  const std::string simulated_group =
      SimulateGroupAssignment(entropy_provider, processed_study);

  // Note: The current (i.e. old) group is checked for the type since that group
  // is the one that should be annotated with the type when killing it.
  const Study_Experiment* experiment = FindExperiment(study, selected_group);
  if (simulated_group != selected_group) {
    if (experiment)
      return ConvertExperimentTypeToChangeType(experiment->type());
    return CHANGED;
  }

  // If the group is unchanged, check whether its params may have changed.
  if (experiment && !VariationParamsAreEqual(study, *experiment))
    return ConvertExperimentTypeToChangeType(experiment->type());

  // Since the group name has not changed and params are either equal or the
  // experiment was not found (and thus there are none), return NO_CHANGE.
  return NO_CHANGE;
}

ChangeType SessionStudyGroupChanged(const ProcessedStudy& processed_study,
                                    const std::string& selected_group) {
  const Study& study = *processed_study.study();
  DCHECK_EQ(Study_Consistency_SESSION, study.consistency());

  const Study_Experiment* experiment = FindExperiment(study, selected_group);
  if (!experiment)
    return CHANGED;
  if (experiment->probability_weight() == 0 &&
      !experiment->has_forcing_flag()) {
    return ConvertExperimentTypeToChangeType(experiment->type());
  }

  // Current group exists in the study - check whether its params changed.
  if (!VariationParamsAreEqual(study, *experiment))
    return ConvertExperimentTypeToChangeType(experiment->type());
  return NO_CHANGE;
}

}  // namespace

SeedSimulationResult ComputeDifferences(
    const std::vector<ProcessedStudy>& processed_studies,
    const VariationsLayers& layers,
    const EntropyProviders& entropy_providers) {
  // Fill in |current_state| with the current process' active field trials, as a
  // map of trial names to group names.
  std::map<std::string, std::string> current_state;
  base::FieldTrial::ActiveGroups trial_groups;
  base::FieldTrialListIncludingLowAnonymity::GetActiveFieldTrialGroups(
      &trial_groups);
  for (auto& group : trial_groups) {
    current_state[group.trial_name] = group.group_name;
  }

  SeedSimulationResult result;
  for (const auto& processed_study : processed_studies) {
    const Study& study = *processed_study.study();
    std::map<std::string, std::string>::const_iterator it =
        current_state.find(study.name());

    // Skip studies that aren't activated in the current state.
    // TODO(asvitkine): This should be handled more intelligently. There are
    // several cases that fall into this category:
    //   1) There's an existing field trial with this name but it is not active.
    //   2) This is a new study config that previously didn't exist.
    // The above cases should be differentiated and handled explicitly.
    if (it == current_state.end())
      continue;

    base::optional_ref<const base::FieldTrial::EntropyProvider>
        entropy_provider = layers.SelectEntropyProviderForStudy(
            processed_study, entropy_providers);
    if (!entropy_provider.has_value()) {
      // Skip if there is no suitable entropy provider to randomize this study.
      continue;
    }

    // Study exists in the current state, check whether its group will change.
    // Note: The logic below does the right thing if study consistency changes,
    // as it doesn't rely on the previous study consistency.
    const std::string& selected_group = it->second;
    ChangeType change_type = NO_CHANGE;
    if (study.consistency() == Study_Consistency_PERMANENT) {
      change_type = PermanentStudyGroupChanged(
          processed_study, selected_group, layers, entropy_provider.value());
    } else if (study.consistency() == Study_Consistency_SESSION) {
      change_type = SessionStudyGroupChanged(processed_study, selected_group);
    }

    switch (change_type) {
      case NO_CHANGE:
        break;
      case CHANGED:
        ++result.normal_group_change_count;
        break;
      case CHANGED_KILL_BEST_EFFORT:
        ++result.kill_best_effort_group_change_count;
        break;
      case CHANGED_KILL_CRITICAL:
        ++result.kill_critical_group_change_count;
        break;
    }
  }

  // TODO(asvitkine): Handle removed studies (i.e. studies that existed in the
  // old seed, but were removed). This will require tracking the set of studies
  // that were created from the original seed.

  return result;
}

SeedSimulationResult SimulateSeedStudies(
    const VariationsSeed& seed,
    const ClientFilterableState& client_state,
    const EntropyProviders& entropy_providers) {
  VariationsLayers layers(seed, entropy_providers);
  auto filtered_studies = FilterAndValidateStudies(seed, client_state, layers);
  return ComputeDifferences(filtered_studies, layers, entropy_providers);
}

}  // namespace variations
