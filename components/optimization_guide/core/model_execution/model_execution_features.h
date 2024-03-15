// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {
namespace features {
namespace internal {

// Features that control the visibility of whether a feature setting is visible
// to the user. Should only be enabled for experimental features that have not
// graduated yet.
BASE_DECLARE_FEATURE(kComposeSettingsVisibility);
BASE_DECLARE_FEATURE(kTabOrganizationSettingsVisibility);
BASE_DECLARE_FEATURE(kWallpaperSearchSettingsVisibility);

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

// Checks if the provided `feature` is graduated from experimental AI settings.
bool IsGraduatedFeature(proto::ModelExecutionFeature feature);

const base::Feature* GetFeatureToUseToCheckSettingsVisibility(
    proto::ModelExecutionFeature feature);

// Returns the features allowed to be shown in the settings UI, and can be
// enabled, even for unsigned users.
base::flat_set<proto::ModelExecutionFeature>
GetAllowedFeaturesForUnsignedUser();

}  // namespace internal
}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_H_
