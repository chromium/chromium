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

BASE_DECLARE_FEATURE(kComposeSettingsVisibility);
BASE_DECLARE_FEATURE(kTabOrganizationSettingsVisibility);
BASE_DECLARE_FEATURE(kWallpaperSearchSettingsVisibility);

// Feature for controlling the users who are eligible to see the IPH promo for
// experimental AI.
BASE_DECLARE_FEATURE(kExperimentalAIIPHPromoRampUp);

// Feature for disabling the model execution user account capability check.
BASE_DECLARE_FEATURE(kModelExecutionCapabilityDisable);

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
