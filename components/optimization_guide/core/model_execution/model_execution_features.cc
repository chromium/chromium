// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide::features::internal {

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
BASE_FEATURE(kHistorySearchSettingsVisibility,
             "HistorySearchSettingsVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPerformanceClassListForHistorySearch(
    &kHistorySearchSettingsVisibility,
    "PerformanceClassListForHistorySearch",
    "*");

// Graduation features.

// Note: ComposeGraduated is enabled by default because the feature is
// country-restricted at runtime.
BASE_FEATURE(kComposeGraduated,
             "ComposeGraduated",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabOrganizationGraduated,
             "TabOrganizationGraduated",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kWallpaperSearchGraduated,
             "WallpaperSearchGraduated",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExperimentalAIIPHPromoRampUp,
             "ExperimentalAIIPHPromoRampUp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kModelExecutionCapabilityDisable,
             "ModelExecutionCapabilityDisable",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kModelAdaptationCompose,
             "ModelAdaptationCompose",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelTestFeature,
             "OnDeviceModelTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kModelAdaptationHistorySearch,
             "ModelAdaptationHistorySearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kModelAdaptationSummarize,
             "ModelAdaptationHistorySearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
    case UserVisibleFeatureKey::kHistorySearch:
      // History search is currently planned to always be opt-in.
      is_graduated = false;
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
    case UserVisibleFeatureKey::kHistorySearch:
      return &kHistorySearchSettingsVisibility;
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

bool ShouldEnableFeatureWhenMainToggleOn(UserVisibleFeatureKey feature) {
  const auto* visibility_feature =
      GetFeatureToUseToCheckSettingsVisibility(feature);
  return (GetFieldTrialParamByFeatureAsBool(
      *visibility_feature, "enable_feature_when_main_toggle_on", true));
}

// LINT.IfChange(IsOnDeviceModelEnabled)
//
// To enable on-device execution for a feature, update this to return a
// non-null target. `GetOnDeviceFeatureRecentlyUsedPref` must also be updated to
// return a valid pref for each on-device feature.
std::optional<proto::OptimizationTarget> GetOptimizationTargetForCapability(
    ModelBasedCapabilityKey feature_key) {
  switch (feature_key) {
    case ModelBasedCapabilityKey::kCompose:
      if (base::FeatureList::IsEnabled(kOptimizationGuideComposeOnDeviceEval)) {
        return proto::OPTIMIZATION_TARGET_COMPOSE;
      }
      return std::nullopt;
    case ModelBasedCapabilityKey::kTest:
      if (base::FeatureList::IsEnabled(kOnDeviceModelTestFeature)) {
        return proto::OPTIMIZATION_TARGET_MODEL_VALIDATION;
      }
      return std::nullopt;
    case ModelBasedCapabilityKey::kPromptApi:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PROMPT_API;
    case ModelBasedCapabilityKey::kSummarize:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SUMMARIZE;
    case ModelBasedCapabilityKey::kHistorySearch:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_HISTORY_SEARCH;
    case ModelBasedCapabilityKey::kHistoryQueryIntent:
      return proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_HISTORY_QUERY_INTENT;
    // The below capabilities never support on-device execution.
    case ModelBasedCapabilityKey::kFormsAnnotations:
    case ModelBasedCapabilityKey::kFormsPredictions:
    case ModelBasedCapabilityKey::kTabOrganization:
    case ModelBasedCapabilityKey::kWallpaperSearch:
    case ModelBasedCapabilityKey::kTextSafety:
    case ModelBasedCapabilityKey::kBlingPrototyping:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/optimization_guide/core/model_execution/model_execution_prefs.cc:GetOnDeviceFeatureRecentlyUsedPref)

}  // namespace optimization_guide::features::internal
