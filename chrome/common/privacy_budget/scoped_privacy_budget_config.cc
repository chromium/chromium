// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"

#include "base/check.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace test {

// Are you happy now linker?
const int ScopedPrivacyBudgetConfig::kDefaultGeneration;
constexpr int kSelectAllSurfacesExpectedSurfaceCount = 1;

ScopedPrivacyBudgetConfig::Parameters::Parameters() = default;
ScopedPrivacyBudgetConfig::Parameters::Parameters(const Parameters&) = default;
ScopedPrivacyBudgetConfig::Parameters::Parameters(Parameters&&) = default;
ScopedPrivacyBudgetConfig::Parameters::~Parameters() = default;

ScopedPrivacyBudgetConfig::Parameters::Parameters(Presets presets) {
  switch (presets) {
    case Presets::kEnableRandomSampling:
      expected_surface_count = kSelectAllSurfacesExpectedSurfaceCount;
      break;

    case Presets::kDisable:
      enabled = false;
      break;
  }
}

ScopedPrivacyBudgetConfig::~ScopedPrivacyBudgetConfig() {
  DCHECK(applied_) << "ScopedPrivacyBudgetConfig instance created but not "
                      "applied. Did you forget to call Apply()?";
}

ScopedPrivacyBudgetConfig::ScopedPrivacyBudgetConfig() = default;

ScopedPrivacyBudgetConfig::ScopedPrivacyBudgetConfig(
    const Parameters& parameters) {
  Apply(parameters);
}

ScopedPrivacyBudgetConfig::ScopedPrivacyBudgetConfig(Presets presets) {
  Apply(Parameters(presets));
}

void ScopedPrivacyBudgetConfig::Apply(const Parameters& parameters) {
  blink::IdentifiabilityStudySettings::ResetStateForTesting();
  DCHECK(!applied_);
  applied_ = true;

  if (!parameters.enabled) {
    scoped_feature_list_.InitAndDisableFeature(features::kIdentifiabilityStudy);
    return;
  }

  Parameters defaults;

  base::FieldTrialParams ftp;

  ftp.insert({features::kIdentifiabilityStudyGeneration.name,
              base::NumberToString(parameters.generation)});

  if (!parameters.blocked_surfaces.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudyBlockedMetrics.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.blocked_surfaces)});
  }
  if (!parameters.blocked_types.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudyBlockedTypes.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.blocked_types)});
  }

  if (!parameters.allowed_random_types.empty()) {
    ftp.insert({features::kIdentifiabilityStudyAllowedRandomTypes.name,
                EncodeIdentifiabilityFieldTrialParam(
                    parameters.allowed_random_types)});
  }

  ftp.insert({features::kIdentifiabilityStudyExpectedSurfaceCount.name,
              base::NumberToString(parameters.expected_surface_count)});

  if (parameters.active_surface_budget != defaults.active_surface_budget) {
    ftp.insert({features::kIdentifiabilityStudyActiveSurfaceBudget.name,
                base::NumberToString(parameters.active_surface_budget)});
  }
  if (!parameters.per_surface_cost.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudyPerHashCost.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.per_surface_cost)});
  }
  if (!parameters.per_type_cost.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudyPerTypeCost.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.per_type_cost)});
  }
  if (!parameters.equivalence_classes.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudySurfaceEquivalenceClasses.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.equivalence_classes)});
  }
  if (!parameters.blocks.empty()) {
    ftp.insert({features::kIdentifiabilityStudyBlocks.name,
                EncodeIdentifiabilityFieldTrialParam(parameters.blocks)});
  }
  if (!parameters.block_weights.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudyBlockWeights.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.block_weights)});
  }
  if (!parameters.per_surface_cost.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudyPerHashCost.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.per_surface_cost)});
  }
  if (!parameters.per_type_cost.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudyPerTypeCost.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.per_type_cost)});
  }
  if (!parameters.equivalence_classes.empty()) {
    ftp.insert(
        {features::kIdentifiabilityStudySurfaceEquivalenceClasses.name,
         EncodeIdentifiabilityFieldTrialParam(parameters.equivalence_classes)});
  }

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kIdentifiabilityStudy, ftp);
}

}  // namespace test
