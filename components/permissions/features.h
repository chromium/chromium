// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_FEATURES_H_
#define COMPONENTS_PERMISSIONS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace permissions {
namespace features {

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockPromptsIfDismissedOften);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockPromptsIfIgnoredOften);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockRepeatedNotificationPermissionPrompts);

COMPONENT_EXPORT(PERMISSIONS_COMMON) BASE_DECLARE_FEATURE(kConfirmationChip);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kChipLocationBarIconOverride);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kNotificationInteractionHistory);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kOneTimeGeolocationPermission);

COMPONENT_EXPORT(PERMISSIONS_COMMON) BASE_DECLARE_FEATURE(kPermissionChip);

COMPONENT_EXPORT(PERMISSIONS_COMMON) BASE_DECLARE_FEATURE(kPermissionQuietChip);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionChipAutoDismiss);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kFailFastQuietChip);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<int> kPermissionChipAutoDismissDelay;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionPredictionServiceUseUrlOverride);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionOnDeviceNotificationPredictions);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionOnDeviceGeolocationPredictions);

#if BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockNotificationPromptsIfDisabledOnAppLevel);

#else

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionsPostPromptSurvey);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kRecordPermissionExpirationTimestamps);

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
namespace feature_params {

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kOkButtonBehavesAsAllowAlways;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionPredictionServiceUrlOverride;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<double>
    kPermissionOnDeviceGeolocationPredictionsHoldbackChance;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<double>
    kPermissionOnDeviceNotificationPredictionsHoldbackChance;

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPostPromptSurveyTriggerId;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPostPromptSurveyRequestTypeFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPostPromptSurveyActionFilter;
#endif

}  // namespace feature_params
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_FEATURES_H_
