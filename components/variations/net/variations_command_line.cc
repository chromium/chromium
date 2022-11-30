// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_command_line.h"

#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/variations_switches.h"

namespace variations {

namespace {

// Format the provided |param_key| and |param_value| as commandline input.
std::string GenerateParam(const std::string& param_key,
                          const std::string& param_value) {
  if (!param_value.empty())
    return " --" + param_key + "=\"" + param_value + "\"";

  return "";
}

}  // namespace

std::string GetVariationsCommandLine() {
  std::string field_trial_states;
  base::FieldTrialList::AllStatesToString(&field_trial_states);

  std::string field_trial_params =
      base::FieldTrialList::AllParamsToString(&EscapeValue);

  std::string enable_features;
  std::string disable_features;
  base::FeatureList::GetInstance()->GetFeatureOverrides(&enable_features,
                                                        &disable_features);

  std::string output;
  output.append(
      GenerateParam(::switches::kForceFieldTrials, field_trial_states));
  output.append(
      GenerateParam(switches::kForceFieldTrialParams, field_trial_params));
  output.append(GenerateParam(::switches::kEnableFeatures, enable_features));
  output.append(GenerateParam(::switches::kDisableFeatures, disable_features));
  return output;
}

}  // namespace variations
