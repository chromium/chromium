// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_params_manager.h"

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
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
    const std::string& trial_group_name,
    const std::map<std::string, std::string>& param_values) {
  base::AssociateFieldTrialParams(trial_name, trial_group_name, param_values);
  return base::FieldTrialList::CreateFieldTrial(trial_name, trial_group_name);
}

}  // namespace

VariationParamsManager::VariationParamsManager()
    : scoped_feature_list_(new base::test::ScopedFeatureList()) {
  scoped_feature_list_->InitWithEmptyFeatureAndFieldTrialLists();
}

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

// static
void VariationParamsManager::SetVariationParams(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values) {
  CreateFieldTrialWithParams(trial_name, kGroupTesting, param_values);
}

// static
void VariationParamsManager::SetVariationParams(
    const std::string& trial_name,
    const std::string& trial_group_name,
    const std::map<std::string, std::string>& param_values) {
  CreateFieldTrialWithParams(trial_name, trial_group_name, param_values);
}

void VariationParamsManager::SetVariationParamsWithFeatureAssociations(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values,
    const std::set<std::string>& associated_features) {
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();

  base::FieldTrial* field_trial =
      CreateFieldTrialWithParams(trial_name, kGroupTesting, param_values);

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
  scoped_feature_list_->InitWithEmptyFeatureAndFieldTrialLists();
}

}  // namespace testing
}  // namespace variations
