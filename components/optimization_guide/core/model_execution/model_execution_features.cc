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
BASE_FEATURE(kComposeSettingsVisibility, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabOrganizationSettingsVisibility,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWallpaperSearchSettingsVisibility,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kHistorySearchSettingsVisibility,
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
BASE_FEATURE(kComposeGraduated, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTabOrganizationGraduated, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kWallpaperSearchGraduated, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kModelExecutionCapabilityDisable,
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

}  // namespace optimization_guide::features::internal
