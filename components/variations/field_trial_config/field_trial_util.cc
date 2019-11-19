// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "components/variations/variations_seed_processor.h"
#include "net/base/escape.h"
#include "ui/base/device_form_factor.h"

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
  if (experiment.is_low_end_device == Study::OPTIONAL_BOOL_MISSING) {
    return false;
  }
  if (base::SysInfo::IsLowEndDevice()) {
    return experiment.is_low_end_device == Study::OPTIONAL_BOOL_FALSE;
  }
  return experiment.is_low_end_device == Study::OPTIONAL_BOOL_TRUE;
}

// Gets current form factor and converts it from enum DeviceFormFactor to enum
// Study_FormFactor.
Study::FormFactor _GetCurrentFormFactor() {
  switch (ui::GetDeviceFormFactor()) {
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return Study::PHONE;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return Study::TABLET;
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return Study::DESKTOP;
  }
}

// Returns true if the experiment config has a missing form_factors or it
// contains the current system's form_factor. Otherwise, it is False.
bool HasFormFactor(const FieldTrialTestingExperiment& experiment) {
  for (size_t i = 0; i < experiment.form_factors_size; ++i) {
    if (experiment.form_factors[i] == _GetCurrentFormFactor())
      return true;
  }
  return experiment.form_factors_size == 0;
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

void AssociateParamsFromExperiment(
    const std::string& study_name,
    const FieldTrialTestingExperiment& experiment,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    base::FeatureList* feature_list) {
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
    DLOG(WARNING) << "Field trial config study skipped: " << study_name
                  << "." << experiment.name
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
// - If no experiments match this platform, do not associate any of them.
void ChooseExperiment(
    const FieldTrialTestingStudy& study,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    Study::Platform platform,
    base::FeatureList* feature_list) {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  const FieldTrialTestingExperiment* chosen_experiment = nullptr;
  for (size_t i = 0; i < study.experiments_size; ++i) {
    const FieldTrialTestingExperiment* experiment = study.experiments + i;
    if (HasPlatform(*experiment, platform)) {
      if (!chosen_experiment &&
          !HasDeviceLevelMismatch(*experiment) &&
          HasFormFactor(*experiment)) {
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

std::string UnescapeValue(const std::string& value) {
  return net::UnescapeURLComponent(
      value, net::UnescapeRule::PATH_SEPARATORS |
                 net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
}

std::string EscapeValue(const std::string& value) {
  // This needs to be the inverse of UnescapeValue in the anonymous namespace
  // above.
  std::string net_escaped_str =
      net::EscapeQueryParamValue(value, true /* use_plus */);

  // net doesn't escape '.' and '*' but UnescapeValue() covers those cases.
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
                                                   &UnescapeValue);
}

void AssociateParamsFromFieldTrialConfig(
    const FieldTrialTestingConfig& config,
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    Study::Platform platform,
    base::FeatureList* feature_list) {
  for (size_t i = 0; i < config.studies_size; ++i) {
    const FieldTrialTestingStudy& study = config.studies[i];
    if (study.experiments_size > 0) {
      ChooseExperiment(study, callback, platform, feature_list);
    } else {
      DLOG(ERROR) << "Unexpected empty study: " << study.name;
    }
  }
}

void AssociateDefaultFieldTrialConfig(
    const VariationsSeedProcessor::UIStringOverrideCallback& callback,
    Study::Platform platform,
    base::FeatureList* feature_list) {
  AssociateParamsFromFieldTrialConfig(kFieldTrialConfig, callback, platform,
                                      feature_list);
}

}  // namespace variations
