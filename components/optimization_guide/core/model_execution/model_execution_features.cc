// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide::features::internal {

// Settings visibility features.
BASE_FEATURE(kComposeSettingsVisibility, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWallpaperSearchSettingsVisibility,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFindsSettingsVisibility, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPasswordChangeSubmission,
             "PasswordChangeSubmissionSettingsVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextualCueingSettingsVisibility,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Graduation features.

// Note: ComposeGraduated is enabled by default because the feature is
// country-restricted at runtime.
BASE_FEATURE(kComposeGraduated, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kWallpaperSearchGraduated, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFindsGraduated, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kModelExecutionCapabilityDisable,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateExecuteRequest, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPreconnectSocketsOnExecuteModel,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGraduatedFeature(UserVisibleFeatureKey feature) {
  bool is_graduated = false;
  switch (feature) {
    // Actual features.
    case UserVisibleFeatureKey::kCompose:
      is_graduated = base::FeatureList::IsEnabled(kComposeGraduated);
      break;
    case UserVisibleFeatureKey::kWallpaperSearch:
      is_graduated = base::FeatureList::IsEnabled(kWallpaperSearchGraduated);
      break;
    case UserVisibleFeatureKey::kPasswordChangeSubmission:
      break;
    case UserVisibleFeatureKey::kFinds:
      is_graduated = base::FeatureList::IsEnabled(kFindsGraduated);
      break;
    case UserVisibleFeatureKey::kContextualCueing:
      // Contextual cueing is currently planned to always be opt-out.
      is_graduated = false;
      break;
  }
  return is_graduated;
}

const base::Feature* GetFeatureToUseToCheckSettingsVisibility(
    UserVisibleFeatureKey feature) {
  switch (feature) {
    case UserVisibleFeatureKey::kCompose:
      return &kComposeSettingsVisibility;
    case UserVisibleFeatureKey::kWallpaperSearch:
      return &kWallpaperSearchSettingsVisibility;
    case UserVisibleFeatureKey::kPasswordChangeSubmission:
      return &kPasswordChangeSubmission;
    case UserVisibleFeatureKey::kFinds:
      return &kFindsSettingsVisibility;
    case UserVisibleFeatureKey::kContextualCueing:
      return &kContextualCueingSettingsVisibility;
  }
}

base::flat_set<UserVisibleFeatureKey> GetAllowedFeaturesForUnsignedUser() {
  std::vector<UserVisibleFeatureKey> allowed_features;
  for (UserVisibleFeatureKey key : kAllUserVisibleFeatureKeys) {
    const auto* feature = GetFeatureToUseToCheckSettingsVisibility(key);
    if (GetFieldTrialParamByFeatureAsBool(*feature, "allow_unsigned_user",
                                          false)) {
      allowed_features.push_back(key);
    }
  }
  return allowed_features;
}

bool ShouldEnableFeatureWhenMainToggleOn(UserVisibleFeatureKey feature_key) {
  const auto* visibility_feature =
      GetFeatureToUseToCheckSettingsVisibility(feature_key);
  return (GetFieldTrialParamByFeatureAsBool(
      *visibility_feature, "enable_feature_when_main_toggle_on", true));
}

}  // namespace optimization_guide::features::internal
