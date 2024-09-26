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
#include "components/variations/study_filtering.h"
#include "components/variations/variations_seed_processor.h"

namespace variations {
namespace {

bool HasPlatform(const FieldTrialTestingExperiment& experiment,
                 Study::Platform platform) {
  for (Study::Platform experiment_platform : experiment.platforms) {
    if (experiment_platform == platform) {
      return true;
    }
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
  for (Study::FormFactor experiment_form_factor : experiment.form_factors) {
    if (experiment_form_factor == current_form_factor) {
      return true;
    }
  }
  return experiment.form_factors.size() == 0;
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
  for (const auto& override_ui_string : experiment.override_ui_string) {
    callback.Run(override_ui_string.name_hash,
                 base::UTF8ToUTF16(override_ui_string.value));
  }
}

// Determines whether an experiment should be skipped or not. An experiment
// should be skipped if it enables or disables a feature that is already
// overridden through the command line.
bool ShouldSkipExperiment(const FieldTrialTestingExperiment& experiment,
                          base::FeatureList* feature_list) {
  for (const auto* enabled_feature : experiment.enable_features) {
    if (feature_list->IsFeatureOverridden(enabled_feature)) {
      return true;
    }
  }
  for (const auto* disabled_feature : experiment.disable_features) {
    if (feature_list->IsFeatureOverridden(disabled_feature)) {
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
  if (experiment.params.size() != 0) {
    base::FieldTrialParams params;
    for (const FieldTrialTestingExperimentParams& param : experiment.params) {
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

  for (const auto* enabled_feature : experiment.enable_features) {
    feature_list->RegisterFieldTrialOverride(
        enabled_feature, base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  }
  for (const auto* disabled_feature : experiment.disable_features) {
    feature_list->RegisterFieldTrialOverride(
        disabled_feature, base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  }

  ApplyUIStringOverrides(experiment, callback);
}

Study::Filter CreateFilter(const FieldTrialTestingExperiment& experiment) {
  Study::Filter filter;
  for (const auto* included_hw_class : experiment.hardware_classes) {
    filter.add_hardware_class(included_hw_class);
  }
  for (const auto* excluded_hw_class : experiment.exclude_hardware_classes) {
    filter.add_exclude_hardware_class(excluded_hw_class);
  }
  return filter;
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
  std::string hardware_class = ClientFilterableState::GetHardwareClass();
  const FieldTrialTestingExperiment* chosen_experiment = nullptr;
  for (const FieldTrialTestingExperiment& experiment : study.experiments) {
    if (HasPlatform(experiment, platform)) {
      Study::Filter filter = CreateFilter(experiment);
      // TODO(b/323589616): These Has*() functions can be replaced by their
      // equivalent internal::CheckStudy* functions once we add the
      // corresponding fields to |CreateFilter|.
      if (!chosen_experiment && !HasDeviceLevelMismatch(experiment) &&
          HasFormFactor(experiment, current_form_factor) &&
          HasMinOSVersion(experiment) &&
          internal::CheckStudyHardwareClass(filter, hardware_class)) {
        chosen_experiment = &experiment;
      }

      if (experiment.forcing_flag &&
          command_line.HasSwitch(experiment.forcing_flag)) {
        chosen_experiment = &experiment;
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
  for (const FieldTrialTestingStudy& study : config.studies) {
    if (study.experiments.size() > 0) {
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
