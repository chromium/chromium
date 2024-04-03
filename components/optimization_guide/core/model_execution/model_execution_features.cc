// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"

namespace optimization_guide {
namespace features {
namespace internal {

// Settings visibility features.
BASE_FEATURE(kComposeSettingsVisibility,
             "ComposeSettingsVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabOrganizationSettingsVisibility,
             "TabOrganizationSettingsVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWallpaperSearchSettingsVisibility,
             "WallpaperSearchSettingsVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Graduation features.
BASE_FEATURE(kComposeGraduated,
             "ComposeGraduated",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabOrganizationGraduated,
             "TabOrganizationGraduated",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWallpaperSearchGraduated,
             "WallpaperSearchGraduated",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExperimentalAIIPHPromoRampUp,
             "ExperimentalAIIPHPromoRampUp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kModelExecutionCapabilityDisable,
             "ModelExecutionCapabilityDisable",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGraduatedFeature(UserVisibleFeatureKey feature) {
  bool is_graduated = false;
  switch (feature) {
    // Actual features.
    case UserVisibleFeatureKey::kCompose:
      is_graduated = base::FeatureList::IsEnabled(kComposeGraduated);
      break;
    case UserVisibleFeatureKey::kTabOrganization:
      is_graduated = base::FeatureList::IsEnabled(kTabOrganizationGraduated);
      break;
    case UserVisibleFeatureKey::kWallpaperSearch:
      is_graduated = base::FeatureList::IsEnabled(kWallpaperSearchGraduated);
      break;
  }
  DCHECK(!is_graduated ||
         !base::FeatureList::IsEnabled(
             *GetFeatureToUseToCheckSettingsVisibility(feature)))
      << "Feature should not be both graduated and visible in settings: "
      << GetFeatureToUseToCheckSettingsVisibility(feature)->name;
  return is_graduated;
}

const base::Feature* GetFeatureToUseToCheckSettingsVisibility(
    UserVisibleFeatureKey feature) {
  switch (feature) {
    case UserVisibleFeatureKey::kCompose:
      return &kComposeSettingsVisibility;
    case UserVisibleFeatureKey::kTabOrganization:
      return &kTabOrganizationSettingsVisibility;
    case UserVisibleFeatureKey::kWallpaperSearch:
      return &kWallpaperSearchSettingsVisibility;
  }
}

base::flat_set<UserVisibleFeatureKey> GetAllowedFeaturesForUnsignedUser() {
  std::vector<UserVisibleFeatureKey> allowed_features;
  for (auto key : kAllUserVisibleFeatureKeys) {
    const auto* feature = GetFeatureToUseToCheckSettingsVisibility(key);
    if (GetFieldTrialParamByFeatureAsBool(*feature, "allow_unsigned_user",
                                          false)) {
      allowed_features.push_back(key);
    }
  }
  return allowed_features;
}

}  // namespace internal

}  // namespace features

}  // namespace optimization_guide
