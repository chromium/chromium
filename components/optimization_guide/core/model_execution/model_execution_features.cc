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
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPasswordChangeSubmission,
             "PasswordChangeSubmissionSettingsVisibility",
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

BASE_FEATURE(kOnDeviceModelTestFeature,
             "OnDeviceModelTestFeature",
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
    case UserVisibleFeatureKey::kHistorySearch:
      // History search is currently planned to always be opt-in.
      is_graduated = false;
      break;
    case UserVisibleFeatureKey::kPasswordChangeSubmission:
      break;
  }
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
    case UserVisibleFeatureKey::kPasswordChangeSubmission:
      return &kPasswordChangeSubmission;
  }
}

base::flat_set<UserVisibleFeatureKey> GetAllowedFeaturesForUnsignedUser() {
  std::vector<UserVisibleFeatureKey> allowed_features;
  for (UserVisibleFeatureKey key : kAllUserVisibleFeatureKeys) {
    const auto* feature = GetFeatureToUseToCheckSettingsVisibility(key);
    // The kHistorySearch feature launched with this param set true,
    // but for other features it defaults to false.
    bool default_value = key == UserVisibleFeatureKey::kHistorySearch;
    if (GetFieldTrialParamByFeatureAsBool(*feature, "allow_unsigned_user",
                                          default_value)) {
      allowed_features.push_back(key);
    }
  }
  return allowed_features;
}

bool ShouldEnableFeatureWhenMainToggleOn(UserVisibleFeatureKey feature_key) {
  const auto* visibility_feature =
      GetFeatureToUseToCheckSettingsVisibility(feature_key);
  // The kHistorySearch feature launched with this param set false,
  // but for other features it defaults to true.
  bool default_value = feature_key != UserVisibleFeatureKey::kHistorySearch;
  return (GetFieldTrialParamByFeatureAsBool(
      *visibility_feature, "enable_feature_when_main_toggle_on",
      default_value));
}

// To enable on-device execution for a feature, update this to return a
// non-null target.
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
    case ModelBasedCapabilityKey::kScamDetection:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SCAM_DETECTION;
    case ModelBasedCapabilityKey::kPermissionsAi:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PERMISSIONS_AI;
    case ModelBasedCapabilityKey::kWritingAssistanceApi:
      return proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API;
    case ModelBasedCapabilityKey::kProofreaderApi:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PROOFREADER_API;
    // The below capabilities never support on-device execution.
    case ModelBasedCapabilityKey::kFormsClassifications:
    case ModelBasedCapabilityKey::kTabOrganization:
    case ModelBasedCapabilityKey::kWallpaperSearch:
    case ModelBasedCapabilityKey::kTextSafety:
    case ModelBasedCapabilityKey::kBlingPrototyping:
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
    case ModelBasedCapabilityKey::kEnhancedCalendar:
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return std::nullopt;
  }
}

}  // namespace optimization_guide::features::internal
