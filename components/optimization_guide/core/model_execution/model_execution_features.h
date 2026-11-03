// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide::features::internal {

// Features that control the visibility of whether a feature setting is visible
// to the user. Should only be enabled for experimental features that have not
// graduated yet.
BASE_DECLARE_FEATURE(kComposeSettingsVisibility);
BASE_DECLARE_FEATURE(kWallpaperSearchSettingsVisibility);
BASE_DECLARE_FEATURE(kFindsSettingsVisibility);
BASE_DECLARE_FEATURE(kContextualCueingSettingsVisibility);

// Features that determine when a feature has graduated from experimental. These
// should not be enabled at the same time as their respective settings
// visibility features.
BASE_DECLARE_FEATURE(kComposeGraduated);
BASE_DECLARE_FEATURE(kWallpaperSearchGraduated);
BASE_DECLARE_FEATURE(kFindsGraduated);

// Feature for disabling the model execution user account capability check.
BASE_DECLARE_FEATURE(kModelExecutionCapabilityDisable);

// Feature flag to populate PrivateExecuteRequest instead of ExecuteRequest.
BASE_DECLARE_FEATURE(kPrivateExecuteRequest);

// Feature to preconnect sockets to the Model Execution Service during
// `ExecuteModel()`.
BASE_DECLARE_FEATURE(kPreconnectSocketsOnExecuteModel);

// Checks if the provided `feature` is graduated from experimental AI settings.
bool IsGraduatedFeature(UserVisibleFeatureKey feature);

const base::Feature* GetFeatureToUseToCheckSettingsVisibility(
    UserVisibleFeatureKey feature);

// Returns the features allowed to be shown in the settings UI, and can be
// enabled, even for unsigned users.
base::flat_set<UserVisibleFeatureKey> GetAllowedFeaturesForUnsignedUser();

// Returns whether the `feature` should get enabled, when the main toggle is on.
bool ShouldEnableFeatureWhenMainToggleOn(UserVisibleFeatureKey feature);

}  // namespace optimization_guide::features::internal

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_
