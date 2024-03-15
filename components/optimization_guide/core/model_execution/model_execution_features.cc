// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"

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

bool IsGraduatedFeature(proto::ModelExecutionFeature feature) {
  bool is_graduated = false;
  switch (feature) {
    // Actual features.
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      is_graduated = base::FeatureList::IsEnabled(kComposeGraduated);
      break;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      is_graduated = base::FeatureList::IsEnabled(kTabOrganizationGraduated);
      break;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      is_graduated = base::FeatureList::IsEnabled(kWallpaperSearchGraduated);
      break;
    // Non-features.
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST:
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      NOTREACHED();
      return false;
  }
  DCHECK(!is_graduated ||
         !base::FeatureList::IsEnabled(
             *GetFeatureToUseToCheckSettingsVisibility(feature)))
      << "Feature should not be both graduated and visible in settings: "
      << GetFeatureToUseToCheckSettingsVisibility(feature)->name;
  return is_graduated;
}

const base::Feature* GetFeatureToUseToCheckSettingsVisibility(
    proto::ModelExecutionFeature feature) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      return &kComposeSettingsVisibility;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      return &kTabOrganizationSettingsVisibility;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      return &kWallpaperSearchSettingsVisibility;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST:
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      NOTREACHED();
      return nullptr;
  }
}

base::flat_set<proto::ModelExecutionFeature>
GetAllowedFeaturesForUnsignedUser() {
  std::vector<proto::ModelExecutionFeature> allowed_features;
  for (int i = proto::ModelExecutionFeature_MIN;
       i <= proto::ModelExecutionFeature_MAX; ++i) {
    proto::ModelExecutionFeature model_execution_feature =
        static_cast<proto::ModelExecutionFeature>(i);
    if (model_execution_feature ==
        proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED) {
      continue;
    }
    if (model_execution_feature ==
        proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST) {
      continue;
    }
    const auto* feature =
        GetFeatureToUseToCheckSettingsVisibility(model_execution_feature);
    if (GetFieldTrialParamByFeatureAsBool(*feature, "allow_unsigned_user",
                                          false)) {
      allowed_features.push_back(model_execution_feature);
    }
  }
  return allowed_features;
}

}  // namespace internal

}  // namespace features

}  // namespace optimization_guide
