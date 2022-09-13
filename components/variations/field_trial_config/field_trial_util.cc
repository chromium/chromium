// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_config/field_trial_util.h"

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "components/variations/variations_seed_processor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations {
namespace {

bool HasPlatform(const FieldTrialTestingExperiment& experiment,
                 Study::Platform platform) {
  for (size_t i = 0; i < experiment.platforms_size; ++i) {
    if (experiment.platforms[i] == platform)
      return true;
  }
  return false;
}

// Returns true if the experiment config has a different value for
// is_low_end_device than the current system value does.
// If experiment has is_low_end_device missing, then it is False.
bool HasDeviceLevelMismatch(const FieldTrialTestingExperiment& experiment) {
  if (!experiment.is_low_end_device.has_value()) {
    return false;
  }
  return experiment.is_low_end_device.value() !=
         base::SysInfo::IsLowEndDevice();
}

// Returns true if the experiment config has a missing form_factors or it
// contains the current system's form_factor. Otherwise, it is False.
bool HasFormFactor(const FieldTrialTestingExperiment& experiment,
                   Study::FormFactor current_form_factor) {
  for (size_t i = 0; i < experiment.form_factors_size; ++i) {
    if (experiment.form_factors[i] == current_form_factor)
      return true;
  }
  return experiment.form_factors_size == 0;
}

// Returns true if the experiment config has a missing |min_os_version| or
// GetOSVersion() >= |min_os_version|.
bool HasMinOSVersion(const FieldTrialTestingExperiment& experiment) {
  if (!experiment.min_os_version)
    return true;
  return base::Version(experiment.min_os_version) <=
         ClientFilterableState::GetOSVersion();
}

// Records the override ui string config. Mainly used for testing.
void ApplyUIStringOverrides(
    const FieldTrialTestingExperiment& experiment,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback) {
  for (size_t i = 0; i < experiment.override_ui_string_size; ++i) {
    callback.Run(experiment.override_ui_string[i].name_hash,
                 base::UTF8ToUTF16(experiment.override_ui_string[i].value));
  }
}

// Determines whether an experiment should be skipped or not. An experiment
// should be skipped if it enables or disables a feature that is already
// overridden through the command line.
bool ShouldSkipExperiment(const FieldTrialTestingExperiment& experiment,
                          base::FeatureList* feature_list) {
  for (size_t i = 0; i < experiment.enable_features_size; ++i) {
    if (feature_list->IsFeatureOverridden(experiment.enable_features[i])) {
      return true;
    }
  }
  for (size_t i = 0; i < experiment.disable_features_size; ++i) {
    if (feature_list->IsFeatureOverridden(experiment.disable_features[i])) {
      return true;
    }
  }
  return false;
}

void AssociateParamsFromExperiment(
    const std::string& study_name,
    const FieldTrialTestingExperiment& experiment,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    base::FeatureList* feature_list) {
  if (ShouldSkipExperiment(experiment, feature_list)) {
    LOG(WARNING) << "Field trial config study skipped: " << study_name << "."
                 << experiment.name
                 << " (some of its features are already overridden)";
    return;
  }
  if (experiment.params_size != 0) {
    base::FieldTrialParams params;
    for (size_t i = 0; i < experiment.params_size; ++i) {
      const FieldTrialTestingExperimentParams& param = experiment.params[i];
      params[param.key] = param.value;
    }
    base::AssociateFieldTrialParams(study_name, experiment.name, params);
  }
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(study_name, experiment.name);

  if (!trial) {
    LOG(WARNING) << "Field trial config study skipped: " << study_name << "."
                 << experiment.name
                 << " (it is overridden from chrome://flags)";
    return;
  }

  for (size_t i = 0; i < experiment.enable_features_size; ++i) {
    feature_list->RegisterFieldTrialOverride(
        experiment.enable_features[i],
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  }
  for (size_t i = 0; i < experiment.disable_features_size; ++i) {
    feature_list->RegisterFieldTrialOverride(
        experiment.disable_features[i],
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  }

  ApplyUIStringOverrides(experiment, callback);
}

// Choose an experiment to associate. The rules are:
// - Out of the experiments which match this platform:
//   - If there is a forcing flag for any experiment, choose the first such
//     experiment.
//   - Otherwise, If running on low_end_device and the config specify
//     a different experiment group for low end devices then pick that.
//   - Otherwise, If running on non low_end_device and the config specify
//     a different experiment group for non low_end_device then pick that.
//   - Otherwise, select the first experiment.
// - The chosen experiment must not enable or disable a feature that is
//   explicitly enabled or disabled through a switch, such as the
//   |--enable-features| or |--disable-features| switches. If it does, then no
//   experiment is associated.
// - If no experiments match this platform, do not associate any of them.
void ChooseExperiment(
    const FieldTrialTestingStudy& study,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    Study::Platform platform,
    Study::FormFactor current_form_factor,
    base::FeatureList* feature_list) {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  const FieldTrialTestingExperiment* chosen_experiment = nullptr;
  for (size_t i = 0; i < study.experiments_size; ++i) {
    const FieldTrialTestingExperiment* experiment = study.experiments + i;
    if (HasPlatform(*experiment, platform)) {
      if (!chosen_experiment && !HasDeviceLevelMismatch(*experiment) &&
          HasFormFactor(*experiment, current_form_factor) &&
          HasMinOSVersion(*experiment)) {
        chosen_experiment = experiment;
      }

      if (experiment->forcing_flag &&
          command_line.HasSwitch(experiment->forcing_flag)) {
        chosen_experiment = experiment;
        break;
      }
    }
  }
  if (chosen_experiment) {
    AssociateParamsFromExperiment(study.name, *chosen_experiment, callback,
                                  feature_list);
  }
}

}  // namespace

std::string EscapeValue(const std::string& value) {
  // This needs to be the inverse of UnescapeValue in
  // base/metrics/field_trial_params.
  std::string net_escaped_str =
      base::EscapeQueryParamValue(value, true /* use_plus */);

  // net doesn't escape '.' and '*' but base::UnescapeValue() covers those
  // cases.
  std::string escaped_str;
  escaped_str.reserve(net_escaped_str.length());
  for (const char ch : net_escaped_str) {
    if (ch == '.')
      escaped_str.append("%2E");
    else if (ch == '*')
      escaped_str.append("%2A");
    else
      escaped_str.push_back(ch);
  }
  return escaped_str;
}

bool AssociateParamsFromString(const std::string& varations_string) {
  return base::AssociateFieldTrialParamsFromString(varations_string,
                                                   &base::UnescapeValue);
}

void AssociateParamsFromFieldTrialConfig(
    const FieldTrialTestingConfig& config,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    Study::Platform platform,
    Study::FormFactor current_form_factor,
    base::FeatureList* feature_list) {
  for (size_t i = 0; i < config.studies_size; ++i) {
    const FieldTrialTestingStudy& study = config.studies[i];
    if (study.experiments_size > 0) {
      ChooseExperiment(study, callback, platform, current_form_factor,
                       feature_list);
    } else {
      DLOG(ERROR) << "Unexpected empty study: " << study.name;
    }
  }
}

void AssociateDefaultFieldTrialConfig(
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    Study::Platform platform,
    Study::FormFactor current_form_factor,
    base::FeatureList* feature_list) {
  AssociateParamsFromFieldTrialConfig(kFieldTrialConfig, callback, platform,
                                      current_form_factor, feature_list);
}

}  // namespace variations
