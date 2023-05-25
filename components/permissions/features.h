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
BASE_DECLARE_FEATURE(kBlockRepeatedAutoReauthnPrompts);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockRepeatedNotificationPermissionPrompts);

COMPONENT_EXPORT(PERMISSIONS_COMMON) BASE_DECLARE_FEATURE(kConfirmationChip);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kChipLocationBarIconOverride);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kNotificationInteractionHistory);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kOneTimePermission);

COMPONENT_EXPORT(PERMISSIONS_COMMON) BASE_DECLARE_FEATURE(kPermissionChip);

COMPONENT_EXPORT(PERMISSIONS_COMMON) BASE_DECLARE_FEATURE(kPermissionQuietChip);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kFailFastQuietChip);

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
BASE_DECLARE_FEATURE(kPermissionsPromptSurvey);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kRecordPermissionExpirationTimestamps);

#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionStorageAccessAPI);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kWindowManagementPermissionAlias);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockMidiByDefault);

}  // namespace features
namespace feature_params {

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kUseStrongerPromptLanguage;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<base::TimeDelta> kOneTimePermissionTimeout;

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
extern const base::FeatureParam<std::string> kPermissionsPromptSurveyTriggerId;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPromptSurveyDisplayTime;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string> kProbabilityVector;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPromptSurveyRequestTypeFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPromptSurveyActionFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPromptSurveyHadGestureFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPromptSurveyPromptDispositionFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPromptSurveyPromptDispositionReasonFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionPromptSurveyReleaseChannelFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<base::TimeDelta>
    kPermissionPromptSurveyIgnoredPromptsMaximumAge;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionPromptSurveyOneTimePromptsDecidedBucket;
#endif

}  // namespace feature_params
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_FEATURES_H_
