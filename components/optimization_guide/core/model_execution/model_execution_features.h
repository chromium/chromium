// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {
namespace features {
namespace internal {

// Features that control the visibility of whether a feature setting is visible
// to the user. Should only be enabled for experimental features that have not
// graduated yet.
BASE_DECLARE_FEATURE(kComposeSettingsVisibility);
BASE_DECLARE_FEATURE(kTabOrganizationSettingsVisibility);
BASE_DECLARE_FEATURE(kWallpaperSearchSettingsVisibility);
BASE_DECLARE_FEATURE(kHistorySearchSettingsVisibility);

// Comma-separated list of performance classes (e.g. "3,4,5") accepted by
// History Search. Use "*" if there is no performance class requirement.
extern const base::FeatureParam<std::string>
    kPerformanceClassListForHistorySearch;

// Features that determine when a feature has graduated from experimental. These
// should not be enabled at the same time as their respective settings
// visibility features.
BASE_DECLARE_FEATURE(kComposeGraduated);
BASE_DECLARE_FEATURE(kTabOrganizationGraduated);
BASE_DECLARE_FEATURE(kWallpaperSearchGraduated);

// Feature for controlling the users who are eligible to see the IPH promo for
// experimental AI.
BASE_DECLARE_FEATURE(kExperimentalAIIPHPromoRampUp);

// Feature for disabling the model execution user account capability check.
BASE_DECLARE_FEATURE(kModelExecutionCapabilityDisable);

// Features that control model adaptation.
BASE_DECLARE_FEATURE(kModelAdaptationCompose);
BASE_DECLARE_FEATURE(kModelAdaptationHistorySearch);
BASE_DECLARE_FEATURE(kModelAdaptationSummarize);

// Allow on-device model support for Test feature, to be used in tests.
BASE_DECLARE_FEATURE(kOnDeviceModelTestFeature);

// Checks if the provided `feature` is graduated from experimental AI settings.
bool IsGraduatedFeature(UserVisibleFeatureKey feature);

const base::Feature* GetFeatureToUseToCheckSettingsVisibility(
    UserVisibleFeatureKey feature);

// Returns the features allowed to be shown in the settings UI, and can be
// enabled, even for unsigned users.
base::flat_set<UserVisibleFeatureKey> GetAllowedFeaturesForUnsignedUser();

// Returns whether the `feature` should get enabled, when the main toggle is on.
bool ShouldEnableFeatureWhenMainToggleOn(UserVisibleFeatureKey feature);

// Returns whether on-device model execution is enabled for the given feature.
bool IsOnDeviceModelEnabled(ModelBasedCapabilityKey feature);

// Returns whether on-device model adaptation is enabled for the given feature.
bool IsOnDeviceModelAdaptationEnabled(ModelBasedCapabilityKey feature);

// Returns the opt target to use for fetching model adaptations for `feature`.
proto::OptimizationTarget GetOptimizationTargetForModelAdaptation(
    ModelBasedCapabilityKey feature);

}  // namespace internal
}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_
