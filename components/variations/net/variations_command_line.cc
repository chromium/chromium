// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_command_line.h"

#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "base/values.h"
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

std::string GetStringFromDict(const base::Value::Dict& dict,
                              std::string_view key) {
  const std::string* s = dict.FindString(key);
  return s ? *s : std::string();
}

}  // namespace

VariationsCommandLine::VariationsCommandLine() = default;
VariationsCommandLine::~VariationsCommandLine() = default;
VariationsCommandLine::VariationsCommandLine(VariationsCommandLine&&) = default;
VariationsCommandLine& VariationsCommandLine::operator=(
    VariationsCommandLine&&) = default;

VariationsCommandLine VariationsCommandLine::GetForCurrentProcess() {
  VariationsCommandLine result;
  base::FieldTrialList::AllStatesToString(&result.field_trial_states);
  result.field_trial_params =
      base::FieldTrialList::AllParamsToString(&EscapeValue);
  base::FeatureList::GetInstance()->GetFeatureOverrides(
      &result.enable_features, &result.disable_features);
  return result;
}

VariationsCommandLine VariationsCommandLine::GetForCommandLine(
    const base::CommandLine& command_line) {
  VariationsCommandLine result;
  result.field_trial_states =
      command_line.GetSwitchValueASCII(::switches::kForceFieldTrials);
  result.field_trial_params =
      command_line.GetSwitchValueASCII(switches::kForceFieldTrialParams);
  result.enable_features =
      command_line.GetSwitchValueASCII(::switches::kEnableFeatures);
  result.disable_features =
      command_line.GetSwitchValueASCII(::switches::kDisableFeatures);
  return result;
}

std::string VariationsCommandLine::ToString() {
  std::string output;
  output.append(
      GenerateParam(::switches::kForceFieldTrials, field_trial_states));
  output.append(
      GenerateParam(switches::kForceFieldTrialParams, field_trial_params));
  output.append(GenerateParam(::switches::kEnableFeatures, enable_features));
  output.append(GenerateParam(::switches::kDisableFeatures, disable_features));
  output.append(" --");
  output.append(switches::kDisableFieldTrialTestingConfig);
  return output;
}

void VariationsCommandLine::ApplyToCommandLine(
    base::CommandLine& command_line) const {
  if (!field_trial_states.empty()) {
    command_line.AppendSwitchASCII(::switches::kForceFieldTrials,
                                   field_trial_states);
  }
  if (!field_trial_params.empty()) {
    command_line.AppendSwitchASCII(switches::kForceFieldTrialParams,
                                   field_trial_params);
  }
  if (!enable_features.empty()) {
    command_line.AppendSwitchASCII(::switches::kEnableFeatures,
                                   enable_features);
  }
  if (!disable_features.empty()) {
    command_line.AppendSwitchASCII(::switches::kDisableFeatures,
                                   disable_features);
  }
  command_line.AppendSwitch(switches::kDisableFieldTrialTestingConfig);
}

void VariationsCommandLine::ApplyToFeatureAndFieldTrialList(
    base::FeatureList* feature_list) const {
  if (!field_trial_params.empty()) {
    variations::AssociateParamsFromString(field_trial_params);
  }
  if (!field_trial_states.empty()) {
    base::FieldTrialList::CreateTrialsFromString(field_trial_states);
  }
  feature_list->InitFromCommandLine(enable_features, disable_features);
}

std::optional<VariationsCommandLine> VariationsCommandLine::ReadFromFile(
    const base::FilePath& file_path) {
  JSONFileValueDeserializer deserializer(file_path);
  std::unique_ptr<base::Value> value = deserializer.Deserialize(
      /*error_code=*/nullptr, /*error_message=*/nullptr);
  if (!value) {
    return std::nullopt;
  }
  base::Value::Dict* dict = value->GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  VariationsCommandLine result;
  result.field_trial_states =
      GetStringFromDict(*dict, ::switches::kForceFieldTrials);
  result.field_trial_params =
      GetStringFromDict(*dict, switches::kForceFieldTrialParams);
  result.enable_features =
      GetStringFromDict(*dict, ::switches::kEnableFeatures);
  result.disable_features =
      GetStringFromDict(*dict, ::switches::kDisableFeatures);
  return result;
}

bool VariationsCommandLine::WriteToFile(const base::FilePath& file_path) const {
  base::Value::Dict dict =
      base::Value::Dict()
          .Set(::switches::kForceFieldTrials, field_trial_states)
          .Set(switches::kForceFieldTrialParams, field_trial_params)
          .Set(::switches::kEnableFeatures, enable_features)
          .Set(::switches::kDisableFeatures, disable_features);
  JSONFileValueSerializer serializer(file_path);
  return serializer.Serialize(dict);
}

}  // namespace variations
