// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/feature_overrides.h"

namespace variations {

FeatureOverrides::FeatureOverrides(base::FeatureList& feature_list)
    : feature_list_(feature_list) {}

FeatureOverrides::~FeatureOverrides() {
  // TODO(crbug.com/379864779): This doesn't play well with potential server-
  // side overrides.
  for (const auto& field_trial_override : field_trial_overrides_) {
    feature_list_->RegisterFieldTrialOverride(
        field_trial_override.feature->name, field_trial_override.override_state,
        field_trial_override.field_trial);
  }
  feature_list_->RegisterExtraFeatureOverrides(
      std::move(overrides_), /*replace_use_default_overrides=*/true);
}

void FeatureOverrides::EnableFeature(const base::Feature& feature) {
  overrides_.emplace_back(
      std::cref(feature),
      base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE);
}

void FeatureOverrides::DisableFeature(const base::Feature& feature) {
  overrides_.emplace_back(
      std::cref(feature),
      base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
}

void FeatureOverrides::OverrideFeatureWithFieldTrial(
    const base::Feature& feature,
    base::FeatureList::OverrideState override_state,
    base::FieldTrial* field_trial) {
  field_trial_overrides_.emplace_back(FieldTrialOverride{
      .feature = raw_ref(feature),
      .override_state = override_state,
      .field_trial = field_trial,
  });
}

}  // namespace variations
