// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_params_manager.h"

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_switches.h"

namespace variations {
namespace testing {
namespace {

// The fixed testing group created in the provided trail when setting up params.
const char kGroupTesting[] = "Testing";

base::FieldTrial* CreateFieldTrialWithParams(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values) {
  AssociateVariationParams(trial_name, kGroupTesting, param_values);
  return base::FieldTrialList::CreateFieldTrial(trial_name, kGroupTesting);
}

}  // namespace

VariationParamsManager::VariationParamsManager()
    : field_trial_list_(new base::FieldTrialList(nullptr)),
      scoped_feature_list_(new base::test::ScopedFeatureList()) {}

VariationParamsManager::VariationParamsManager(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values)
    : VariationParamsManager() {
  SetVariationParams(trial_name, param_values);
}

VariationParamsManager::VariationParamsManager(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values,
    const std::set<std::string>& associated_features)
    : VariationParamsManager() {
  SetVariationParamsWithFeatureAssociations(trial_name, param_values,
                                            associated_features);
}

VariationParamsManager::~VariationParamsManager() {
  ClearAllVariationIDs();
  ClearAllVariationParams();
}

void VariationParamsManager::SetVariationParams(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values) {
  CreateFieldTrialWithParams(trial_name, param_values);
}

void VariationParamsManager::SetVariationParamsWithFeatureAssociations(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values,
    const std::set<std::string>& associated_features) {
  base::FieldTrial* field_trial =
      CreateFieldTrialWithParams(trial_name, param_values);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  for (const std::string& feature_name : associated_features) {
    feature_list->RegisterFieldTrialOverride(
              feature_name,
              base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
              field_trial);
  }

  scoped_feature_list_->InitWithFeatureList(std::move(feature_list));
}

void VariationParamsManager::ClearAllVariationIDs() {
  testing::ClearAllVariationIDs();
}

void VariationParamsManager::ClearAllVariationParams() {
  testing::ClearAllVariationParams();
  // When the scoped feature list is destroyed, it puts back the original
  // feature list that was there when InitWithFeatureList() was called.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  // Ensure the destructor is called properly, so it can be freshly recreated.
  field_trial_list_.reset();
  field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
}

// static
void VariationParamsManager::AppendVariationParams(
    const std::string& trial_name,
    const std::string& trial_group_name,
    const std::map<std::string, std::string>& param_values,
    base::CommandLine* command_line) {
  // Register the trial group.
  command_line->AppendSwitchASCII(
      ::switches::kForceFieldTrials,
      EscapeValue(trial_name) + "/" + EscapeValue(trial_group_name));

  // Associate |param_values| with the trial group.
  std::string params_arg =
      EscapeValue(trial_name) + "." + EscapeValue(trial_group_name) + ":";
  bool first = true;
  for (const auto& param : param_values) {
    // Separate each |param|.
    if (!first)
      params_arg += "/";
    first = false;

    // Append each |param|.
    const std::string& name = param.first;
    const std::string& value = param.second;
    params_arg += EscapeValue(name) + "/" + EscapeValue(value);
  }
  command_line->AppendSwitchASCII(variations::switches::kForceFieldTrialParams,
                                  params_arg);
}

}  // namespace testing
}  // namespace variations
