// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include <stddef.h>

#include <map>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/processed_study.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_layers.h"

namespace variations {

namespace internal {

const char kFeatureConflictGroupName[] = "ClientSideFeatureConflict";
const char kGoogleGroupFeatureParamName[] = "__GGIDS";
const char kGoogleGroupFeatureParamSeparator[] = ",";

}  // namespace internal

namespace {

// Serializes the `google_groups` attribute of `filter`.
std::string SerializeGoogleGroupsFilter(const Study::Filter& filter) {
  std::string result;
  for (int64_t group_id : filter.google_group()) {
    if (!result.empty()) {
      result.append(internal::kGoogleGroupFeatureParamSeparator);
    }
    result.append(base::NumberToString(group_id));
  }
  return result;
}

// Associates the variations params of `experiment`, if present.
void RegisterExperimentParams(const Study& study,
                              const Study::Experiment& experiment) {
  std::map<std::string, std::string> params;
  for (const auto& param : experiment.param()) {
    if (param.has_name() && param.has_value()) {
      params[param.name()] = param.value();
    }
  }
  // If the study has a filter with a `google_groups` attribute, we write those
  // Google Group ids into a feature parameter. This allows looking up which
  // Google Groups may influence a feature's state.
  if (study.filter().google_group_size() > 0) {
    params[internal::kGoogleGroupFeatureParamName] =
        SerializeGoogleGroupsFilter(study.filter());
  }
  if (!params.empty())
    base::AssociateFieldTrialParams(study.name(), experiment.name(), params);
}

// Returns the IDCollectionKey with which |experiment| should be associated.
// Returns nullopt when |experiment| doesn't have a Google web or Google web
// trigger experiment ID.
std::optional<IDCollectionKey> GetKeyForWebExperiment(
    const Study::Experiment& experiment) {
  if (!VariationsSeedProcessor::HasGoogleWebExperimentId(experiment)) {
    return std::nullopt;
  }
  bool has_web_experiment_id = experiment.has_google_web_experiment_id();
  bool has_web_trigger_experiment_id =
      experiment.has_google_web_trigger_experiment_id();

  // An experiment cannot have both |google_web_experiment_id| and
  // |google_trigger_web_experiment_id|. This is enforced by the variations
  // server before generating a variations seed.
  CHECK(!(has_web_experiment_id && has_web_trigger_experiment_id));

  Study::GoogleWebVisibility visibility = experiment.google_web_visibility();
  if (visibility == Study::FIRST_PARTY) {
    return has_web_trigger_experiment_id
               ? GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY
               : GOOGLE_WEB_PROPERTIES_FIRST_PARTY;
  }
  return has_web_trigger_experiment_id
             ? GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT
             : GOOGLE_WEB_PROPERTIES_ANY_CONTEXT;
}

// If there are VariationIDs associated with |experiment|, register the
// VariationIDs. When `is_trial_overridden` is true, this does not register
// `google_web_experiment_id` as it would have no effect, and would impact
// collected metrics.
void RegisterVariationIds(const Study::Experiment& experiment,
                          const std::string& trial_name,
                          bool is_trial_overridden) {
  if (is_trial_overridden && experiment.has_google_web_experiment_id()) {
    Study::Experiment updated_experiment = experiment;
    updated_experiment.clear_google_web_experiment_id();
    RegisterVariationIds(updated_experiment, trial_name, false);
    return;
  }

  if (experiment.has_google_app_experiment_id()) {
    const VariationID variation_id =
        static_cast<VariationID>(experiment.google_app_experiment_id());
    AssociateGoogleVariationIDForce(GOOGLE_APP, trial_name, experiment.name(),
                                    variation_id);
  }

  std::optional<IDCollectionKey> key = GetKeyForWebExperiment(experiment);
  if (!key.has_value())
    return;

  CHECK(VariationsSeedProcessor::HasGoogleWebExperimentId(experiment));
  // An experiment cannot have both |google_web_experiment_id| and
  // |google_trigger_web_experiment_id|. See GetKeyForWebExperiment() for more
  // details.
  const VariationID variation_id =
      experiment.has_google_web_trigger_experiment_id()
          ? static_cast<VariationID>(
                experiment.google_web_trigger_experiment_id())
          : static_cast<VariationID>(experiment.google_web_experiment_id());

  AssociateGoogleVariationIDForce(key.value(), trial_name, experiment.name(),
                                  variation_id);
}

// Executes |callback| on every override defined by |experiment|.
void ApplyUIStringOverrides(
    const Study::Experiment& experiment,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback) {
  for (int i = 0; i < experiment.override_ui_string_size(); ++i) {
    const Study::Experiment::OverrideUIString& override =
        experiment.override_ui_string(i);
    callback.Run(override.name_hash(), base::UTF8ToUTF16(override.value()));
  }
}

// Forces the specified |experiment| to be enabled in |study|.
void ForceExperimentState(
    const Study& study,
    const Study::Experiment& experiment,
    const VariationsSeedProcessor::UIStringOverrideCallback& override_callback,
    base::FieldTrial* trial) {
  RegisterExperimentParams(study, experiment);
  RegisterVariationIds(experiment, study.name(), trial->IsOverridden());
  if (study.activation_type() == Study::ACTIVATE_ON_STARTUP) {
    // This call must happen after all params have been registered for the
    // trial. Otherwise, since we look up params by trial and group name, the
    // params won't be registered under the correct key.
    trial->Activate();
    // UI Strings can only be overridden from ACTIVATE_ON_STARTUP experiments.
    ApplyUIStringOverrides(experiment, override_callback);
  }
}

// Associates features for groups that do not specify them manually.
void AssociateDefaultFeatures(const Study& study,
                              base::FieldTrial* trial,
                              base::FeatureList* feature_list) {
  // Note: We only compute feature associations for ACTIVATE_ON_QUERY studies,
  // since these associations are only used to determine that the trial has
  // been queried when the feature is queried.
  if (study.activation_type() != Study_ActivationType_ACTIVATE_ON_QUERY)
    return;

  std::set<std::string> features_to_associate;
  for (const auto& experiment : study.experiment()) {
    const auto& features = experiment.feature_association();
    for (const auto& feature : features.enable_feature()) {
      features_to_associate.insert(feature);
    }
    for (const auto& feature : features.disable_feature()) {
      features_to_associate.insert(feature);
    }
  }
  for (const auto& feature_name : features_to_associate) {
    feature_list->RegisterFieldTrialOverride(
        feature_name, base::FeatureList::OVERRIDE_USE_DEFAULT, trial);
  }
}

// Registers feature overrides `experiment` in the `study`.
void RegisterFeatureOverrides(const Study& study,
                              const Study::Experiment& experiment,
                              base::FieldTrial* trial,
                              base::FeatureList* feature_list) {
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
    AssociateDefaultFeatures(study, trial, feature_list);
  }
}

// Checks if |experiment| is associated with a forcing flag or feature and if
// it is, returns whether it should be forced enabled based on the
// |command_line| or |feature_list| state.
bool ShouldForceExperiment(const Study::Experiment& experiment,
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
  if (experiment.has_forcing_flag()) {
    return command_line.HasSwitch(experiment.forcing_flag());
  }
  return false;
}

bool StudyIsLowAnonymity(const Study& study) {
  // Studies which are set based on Google group membership are potentially
  // low anonymity (as the groups could in theory have a small number of
  // members).
  return study.filter().google_group_size() > 0;
}

// Creates a placeholder trial that indicates the feature conflict.
//
// This forcibly associates |trial_name| with the |kFeatureConflictGroupName|
// group, which indicates the trial was not applied due to a feature conflict.
// This group has no features, params, or variation IDs associated with it.
//
// Trials may be associated with this group due to toggling flags in
// chrome://flags that are associated with the trial's features, or if there
// are different trials associated with the same feature.
void CreateTrialWithFeatureConflictGroup(const Study& study) {
  base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
      study.name(), internal::kFeatureConflictGroupName,
      StudyIsLowAnonymity(study));
  DCHECK(trial);
  // Activate immediately to make the conflict obvious in metrics logs.
  trial->Activate();
}

}  // namespace

// static
bool VariationsSeedProcessor::HasGoogleWebExperimentId(
    const Study::Experiment& experiment) {
  return experiment.has_google_web_experiment_id() ||
         experiment.has_google_web_trigger_experiment_id();
}

VariationsSeedProcessor::VariationsSeedProcessor() = default;

VariationsSeedProcessor::~VariationsSeedProcessor() = default;

void VariationsSeedProcessor::CreateTrialsFromSeed(
    const VariationsSeed& seed,
    const ClientFilterableState& client_state,
    const UIStringOverrideCallback& override_callback,
    const EntropyProviders& entropy_providers,
    const VariationsLayers& layers,
    base::FeatureList* feature_list) {
  base::UmaHistogramCounts1000("Variations.AppliedSeed.StudyCount",
                               seed.study().size());
  std::vector<ProcessedStudy> filtered_studies =
      FilterAndValidateStudies(seed, client_state, layers);

  for (const ProcessedStudy& study : filtered_studies) {
    CreateTrialFromStudy(study, override_callback, entropy_providers, layers,
                         feature_list);
  }
}

void VariationsSeedProcessor::CreateTrialFromStudy(
    const ProcessedStudy& processed_study,
    const UIStringOverrideCallback& override_callback,
    const EntropyProviders& entropy_providers,
    const VariationsLayers& layers,
    base::FeatureList* feature_list) {
  // Since trials and features can come from many different sources (variations
  // seed, about://flags, and command line), there are special cases for when
  // they conflict with each other. See the following doc:
  // https://docs.google.com/document/d/1PAlx0KyjRwLJsmkIWlZMgZ-R422Oetgxa3ZPq0Q98aQ

  const Study& study = *processed_study.study();

  // If the trial already exists, check if the selected group exists in the
  // |processed_study|. If not, there is nothing to do here.
  base::FieldTrial* existing_trial = base::FieldTrialList::Find(study.name());
  if (existing_trial) {
    int experiment_index = processed_study.GetExperimentIndexByName(
        existing_trial->GetGroupNameWithoutActivation());
    if (experiment_index == -1) {
      return;
    }

    // If the selected group exists in |processed_study|, then there may be some
    // variation ids, params, and features to pick up, so do not return early.
    // For example, if a user specifies the command line flag
    // "--force-fieldtrials=Study/Enabled" and the variations seed includes
    // a "Study" trial with an "Enabled" group that specifies features or other
    // details, then use those details, even though they were not directly
    // specified on the command line.
  } else {
    // If an experiment group in the study specifies a feature that is already
    // associated with another trial, forcibly select the
    // |kFeatureConflictGroupName| group to indicate a conflict. Usually, the
    // server-side enforces that no two studies enable/disable the same feature,
    // but this might happen from the client-side, such as through flags or
    // through the command line.
    //
    // Only check for this if the trial does not already exist. If it already
    // exists, then we cannot create the |kFeatureConflictGroupName| group for
    // it.
    for (const Study::Experiment& experiment : study.experiment()) {
      const auto& features = experiment.feature_association();
      for (const std::string& feature_name : features.enable_feature()) {
        if (feature_list->HasAssociatedFieldTrialByFeatureName(feature_name)) {
          CreateTrialWithFeatureConflictGroup(study);
          return;
        }
      }
      for (const std::string& feature_name : features.disable_feature()) {
        if (feature_list->HasAssociatedFieldTrialByFeatureName(feature_name)) {
          CreateTrialWithFeatureConflictGroup(study);
          return;
        }
      }
    }
  }

  // Check if any experiments need to be forced due to a command line
  // flag. Force the first experiment with an existing flag.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  for (const auto& experiment : study.experiment()) {
    if (ShouldForceExperiment(experiment, *command_line, *feature_list)) {
      base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
          study.name(), experiment.name(), StudyIsLowAnonymity(study));
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

  // This study has no randomized experiments and none of its experiments were
  // forced by flags so don't create a field trial.
  if (processed_study.total_probability() <= 0) {
    return;
  }

  base::optional_ref<const base::FieldTrial::EntropyProvider> entropy_provider =
      layers.SelectEntropyProviderForStudy(processed_study, entropy_providers);
  if (!entropy_provider.has_value()) {
    // Do not randomize because no suitable entropy provider can be applied to
    // the study.
    return;
  }

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          study.name(), processed_study.total_probability(),
          processed_study.GetDefaultExperimentName(), entropy_provider.value(),
          study.randomization_seed(), StudyIsLowAnonymity(study)));

  bool has_overrides = false;
  bool enables_or_disables_features = false;
  for (const auto& experiment : study.experiment()) {
    // Groups with forcing flags have probability 0 and will never be selected.
    // Therefore, there's no need to add them to the field trial.
    if (experiment.has_forcing_flag() ||
        experiment.feature_association().has_forcing_feature_on() ||
        experiment.feature_association().has_forcing_feature_off()) {
      continue;
    }

    if (experiment.name() != study.default_experiment_name())
      trial->AppendGroup(experiment.name(), experiment.probability_weight());

    RegisterVariationIds(experiment, study.name(),
                         /*is_trial_overridden=*/existing_trial &&
                             existing_trial->IsOverridden());

    has_overrides = has_overrides || experiment.override_ui_string_size() > 0;
    if (experiment.feature_association().enable_feature_size() != 0 ||
        experiment.feature_association().disable_feature_size() != 0) {
      enables_or_disables_features = true;
    }
  }

  trial->SetForced();

  {
    const std::string& group_name = trial->GetGroupNameWithoutActivation();
    int experiment_index = processed_study.GetExperimentIndexByName(group_name);
    // If the trial was forced on the command line, we may not be able to find
    // the experiment.
    if (experiment_index != -1) {
      const Study::Experiment& experiment = study.experiment(experiment_index);
      RegisterExperimentParams(study, experiment);
      if (enables_or_disables_features) {
        RegisterFeatureOverrides(study, experiment, trial.get(), feature_list);
      }
    }
  }

  if (study.activation_type() == Study::ACTIVATE_ON_STARTUP) {
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
    // Although not normally expected, but could happen if the trial was forced
    // on the command line.
    if (experiment_index != -1) {
      ApplyUIStringOverrides(study.experiment(experiment_index),
                             override_callback);
    }
  }
}

}  // namespace variations
