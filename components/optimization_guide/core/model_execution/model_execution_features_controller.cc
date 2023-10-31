// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"

#include "base/logging.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace internal {

OptimizationGuideModelExecutionFeaturesController::
    OptimizationGuideModelExecutionFeaturesController(
        PrefService* browser_context_profile_service)
    : browser_context_profile_service_(browser_context_profile_service) {
  CHECK(browser_context_profile_service_);
}

bool OptimizationGuideModelExecutionFeaturesController::IsSettingEnabled(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      NOTREACHED();
      return false;
    default:
      return browser_context_profile_service_->GetInteger(
                 prefs::GetSettingEnabledPrefName(feature)) ==
             static_cast<int>(prefs::FeatureOptInState::kEnabled);
  }
  NOTREACHED();
  return false;
}

bool OptimizationGuideModelExecutionFeaturesController::IsSettingVisible(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If the setting is currently enabled by user, then we should show the
  // setting to the user regardless of any other checks.
  if (IsSettingEnabled(feature)) {
    return true;
  }

  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      return false;
    default:
      return base::FeatureList::IsEnabled(
          *features::internal::GetFeatureToUseToCheckSettingsVisibility(
              feature));
  }
}

}  // namespace internal

}  // namespace optimization_guide
