// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include <stddef.h>

#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_associated_data.h"

namespace variations {

namespace {

// Associates the variations params of |experiment|, if present.
void RegisterExperimentParams(const Study& study,
                              const Study_Experiment& experiment) {
  std::map<std::string, std::string> params;
  for (int i = 0; i < experiment.param_size(); ++i) {
    if (experiment.param(i).has_name() && experiment.param(i).has_value())
      params[experiment.param(i).name()] = experiment.param(i).value();
  }
  if (!params.empty())
    AssociateVariationParams(study.name(), experiment.name(), params);
}

// If there are variation ids associated with |experiment|, register the
// variation ids.
void RegisterVariationIds(const Study_Experiment& experiment,
                          const std::string& trial_name) {
  if (experiment.has_google_web_experiment_id()) {
    const VariationID variation_id =
        static_cast<VariationID>(experiment.google_web_experiment_id());
    AssociateGoogleVariationIDForce(GOOGLE_WEB_PROPERTIES,
                                    trial_name,
                                    experiment.name(),
                                    variation_id);
  }
  if (experiment.has_google_web_trigger_experiment_id()) {
    const VariationID variation_id =
        static_cast<VariationID>(experiment.google_web_trigger_experiment_id());
    AssociateGoogleVariationIDForce(GOOGLE_WEB_PROPERTIES_TRIGGER,
                                    trial_name,
                                    experiment.name(),
                                    variation_id);
  }
}

// Executes |callback| on every override defined by |experiment|.
void ApplyUIStringOverrides(
    const Study_Experiment& experiment,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback) {
  UMA_HISTOGRAM_COUNTS_100("Variations.StringsOverridden",
                           experiment.override_ui_string_size());
  for (int i = 0; i < experiment.override_ui_string_size(); ++i) {
    const Study_Experiment_OverrideUIString& override =
        experiment.override_ui_string(i);
    callback.Run(override.name_hash(), base::UTF8ToUTF16(override.value()));
  }
}

// Forces the specified |experiment| to be enabled in |study|.
void ForceExperimentState(
    const Study& study,
    const Study_Experiment& experiment,
    const VariationsSeedProcessor::UIStringOverrideCallback& override_callback,
    base::FieldTrial* trial) {
  RegisterExperimentParams(study, experiment);
  RegisterVariationIds(experiment, study.name());
  if (study.activation_type() == Study_ActivationType_ACTIVATE_ON_STARTUP) {
    // This call must happen after all params have been registered for the
    // trial. Otherwise, since we look up params by trial and group name, the
    // params won't be registered under the correct key.
    trial->group();
    // UI Strings can only be overridden from ACTIVATE_ON_STARTUP experiments.
    ApplyUIStringOverrides(experiment, override_callback);
  }
}

// Registers feature overrides for the chosen experiment in the specified study.
void RegisterFeatureOverrides(const ProcessedStudy& processed_study,
                              base::FieldTrial* trial,
                              base::FeatureList* feature_list) {
  const std::string& group_name = trial->GetGroupNameWithoutActivation();
  int experiment_index = processed_study.GetExperimentIndexByName(group_name);
  // If the chosen experiment was not found in the study, simply return.
  // Although not normally expected, but could happen in exception cases, see
  // tests: ExpiredStudy_NoDefaultGroup, ExistingFieldTrial_ExpiredByConfig
  if (experiment_index == -1)
    return;

  const Study& study = *processed_study.study();
  const Study_Experiment& experiment = study.experiment(experiment_index);

  // Process all the features to enable.
  int feature_count = experiment.feature_association().enable_feature_size();
  for (int i = 0; i < feature_count; ++i) {
    feature_list->RegisterFieldTrialOverride(
        experiment.feature_association().enable_feature(i),
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  }

  // Process all the features to disable.
  feature_count = experiment.feature_association().disable_feature_size();
  for (int i = 0; i < feature_count; ++i) {
    feature_list->RegisterFieldTrialOverride(
        experiment.feature_association().disable_feature(i),
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  }

  // Associate features for groups that do not specify them manually (e.g.
  // "Default" group), so that such groups are reported.
  if (!experiment.has_feature_association()) {
    for (const auto& feature_name : processed_study.associated_features()) {
      feature_list->RegisterFieldTrialOverride(
          feature_name, base::FeatureList::OVERRIDE_USE_DEFAULT, trial);
    }
  }
}

// Checks if |experiment| is associated with a forcing flag or feature and if it
// is, returns whether it should be forced enabled based on the |command_line|
// or |feature_list| state.
bool ShouldForceExperiment(const Study_Experiment& experiment,
                           const base::CommandLine& command_line,
                           const base::FeatureList& feature_list) {
  if (experiment.feature_association().has_forcing_feature_on()) {
    return feature_list.IsFeatureOverriddenFromCommandLine(
        experiment.feature_association().forcing_feature_on(),
        base::FeatureList::OVERRIDE_ENABLE_FEATURE);
  }
  if (experiment.feature_association().has_forcing_feature_off()) {
    return feature_list.IsFeatureOverriddenFromCommandLine(
        experiment.feature_association().forcing_feature_off(),
        base::FeatureList::OVERRIDE_DISABLE_FEATURE);
  }
  if (experiment.has_forcing_flag())
    return command_line.HasSwitch(experiment.forcing_flag());
  return false;
}

}  // namespace

VariationsSeedProcessor::VariationsSeedProcessor() {
}

VariationsSeedProcessor::~VariationsSeedProcessor() {
}

void VariationsSeedProcessor::CreateTrialsFromSeed(
    const VariationsSeed& seed,
    const ClientFilterableState& client_state,
    const UIStringOverrideCallback& override_callback,
    const base::FieldTrial::EntropyProvider* low_entropy_provider,
    base::FeatureList* feature_list) {
  std::vector<ProcessedStudy> filtered_studies;
  FilterAndValidateStudies(seed, client_state, &filtered_studies);
  SetSeedVersion(seed.version());

  for (const ProcessedStudy& study : filtered_studies) {
    CreateTrialFromStudy(study, override_callback, low_entropy_provider,
                         feature_list);
  }
}

// static
bool VariationsSeedProcessor::ShouldStudyUseLowEntropy(const Study& study) {
  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (experiment.has_google_web_experiment_id() ||
        experiment.has_google_web_trigger_experiment_id() ||
        experiment.has_chrome_sync_experiment_id()) {
      return true;
    }
  }
  return false;
}

void VariationsSeedProcessor::CreateTrialFromStudy(
    const ProcessedStudy& processed_study,
    const UIStringOverrideCallback& override_callback,
    const base::FieldTrial::EntropyProvider* low_entropy_provider,
    base::FeatureList* feature_list) {
  const Study& study = *processed_study.study();

  // If the trial already exists, check if the selected group exists in the
  // |processed_study|. If not, there is nothing to do here.
  base::FieldTrial* existing_trial = base::FieldTrialList::Find(study.name());
  if (existing_trial) {
    int experiment_index = processed_study.GetExperimentIndexByName(
        existing_trial->GetGroupNameWithoutActivation());
    if (experiment_index == -1)
      return;
  }

  // Check if any experiments need to be forced due to a command line
  // flag. Force the first experiment with an existing flag.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (ShouldForceExperiment(experiment, *command_line, *feature_list)) {
      base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
          study.name(), experiment.name());
      // If |trial| is null, then there might already be a trial forced to a
      // different group (e.g. via --force-fieldtrials). Break out of the loop,
      // but don't return, so that variation ids and params for the selected
      // group will still be picked up.
      if (!trial)
        break;

      if (experiment.feature_association().has_forcing_feature_on()) {
        feature_list->AssociateReportingFieldTrial(
            experiment.feature_association().forcing_feature_on(),
            base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
      } else if (experiment.feature_association().has_forcing_feature_off()) {
        feature_list->AssociateReportingFieldTrial(
            experiment.feature_association().forcing_feature_off(),
            base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
      }
      ForceExperimentState(study, experiment, override_callback, trial);
      return;
    }
  }

  uint32_t randomization_seed = 0;
  base::FieldTrial::RandomizationType randomization_type =
      base::FieldTrial::SESSION_RANDOMIZED;
  if (study.has_consistency() &&
      study.consistency() == Study_Consistency_PERMANENT &&
      // If all assignments are to a single group, no need to enable one time
      // randomization (which is more expensive to compute), since the result
      // will be the same.
      !processed_study.all_assignments_to_one_group()) {
    randomization_type = base::FieldTrial::ONE_TIME_RANDOMIZED;
    if (study.has_randomization_seed())
      randomization_seed = study.randomization_seed();
  }

  // The trial is created without specifying an expiration date because the
  // expiration check in field_trial.cc is based on the build date. Instead,
  // the expiration check using |reference_date| is done explicitly below.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrialWithRandomizationSeed(
          study.name(), processed_study.total_probability(),
          processed_study.GetDefaultExperimentName(), randomization_type,
          randomization_seed, nullptr,
          ShouldStudyUseLowEntropy(study) ? low_entropy_provider : nullptr));

  bool has_overrides = false;
  bool enables_or_disables_features = false;
  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    RegisterExperimentParams(study, experiment);

    // Groups with forcing flags have probability 0 and will never be selected.
    // Therefore, there's no need to add them to the field trial.
    if (experiment.has_forcing_flag() ||
        experiment.feature_association().has_forcing_feature_on() ||
        experiment.feature_association().has_forcing_feature_off()) {
      continue;
    }

    if (experiment.name() != study.default_experiment_name())
      trial->AppendGroup(experiment.name(), experiment.probability_weight());

    RegisterVariationIds(experiment, study.name());

    has_overrides = has_overrides || experiment.override_ui_string_size() > 0;
    if (experiment.feature_association().enable_feature_size() != 0 ||
        experiment.feature_association().disable_feature_size() != 0) {
      enables_or_disables_features = true;
    }
  }

  trial->SetForced();
  if (processed_study.is_expired())
    trial->Disable();

  if (enables_or_disables_features)
    RegisterFeatureOverrides(processed_study, trial.get(), feature_list);

  if (study.activation_type() == Study_ActivationType_ACTIVATE_ON_STARTUP) {
    // This call must happen after all params have been registered for the
    // trial. Otherwise, since we look up params by trial and group name, the
    // params won't be registered under the correct key.
    const std::string& group_name = trial->group_name();

    // Don't try to apply overrides if none of the experiments in this study had
    // any.
    if (!has_overrides)
      return;

    // UI Strings can only be overridden from ACTIVATE_ON_STARTUP experiments.
    int experiment_index = processed_study.GetExperimentIndexByName(group_name);
    // If the chosen experiment was not found in the study, simply return.
    // Although not normally expected, but could happen in exception cases, see
    // tests: ExpiredStudy_NoDefaultGroup, ExistingFieldTrial_ExpiredByConfig
    if (experiment_index != -1) {
      ApplyUIStringOverrides(study.experiment(experiment_index),
                             override_callback);
    }
  }
}

}  // namespace variations
