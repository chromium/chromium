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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kAndroidWindowManagementWebApi);
#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBackForwardCacheUnblockPermissionRequest);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kKeyboardLockPrompt);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionElementPromptPositioning);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionHeuristicAutoGrant);

// DO NOT REMOVE THIS FLAG.
// This feature was used to enable the V2 version of the permission predictions
// model. It is enabled by default. This flag is kept around to be able to
// fetch the size of the holdback group that is provided in the experiment
// parameters.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionPredictionsV2);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionsAIv3);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionsAIv4);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionsAIP92);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionPromiseLifetimeModulation);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionsPromptSurvey);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kAllowMultipleOriginsForWebKioskPermissions);

// DO NOT REMOVE THIS FLAG.
// The following 2 features are enabled by default, but the feature flags are
// kept for internal testing purposes. Specifically, the model
// modelconfig_feature_disable_test will fail without the models being guarded
// by these flags, as the model gets fetched even when the feature is disabled
// in the tests.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionOnDeviceNotificationPredictions);

// DO NOT REMOVE THIS FLAG.
// See comment above.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionOnDeviceGeolocationPredictions);

#if BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionDedicatedCpssSettingAndroid);

#else

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kRecordChooserPermissionLastVisitedTimestamps);

#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionSiteSettingsRadioButton);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kShowRelatedWebsiteSetsPermissionGrants);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kCpssQuietChipTextUpdate);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kCpssUseTfliteSignatureRunner);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kSafetyHubUnusedPermissionRevocationForAllSurfaces);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kGlicActorPermissionsAutoReject);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kReturnDeniedForNotificationsWhenNoAppLevelSettings);
#endif

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionPredictionsGeolocationAccuracy);

}  // namespace features

namespace feature_params {

enum class PermissionElementPromptPosition {
  kWindowMiddle,
  kNearElement,
  kLegacyPrompt,
};

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<PermissionElementPromptPosition>
    kPermissionElementPromptPositioningParam;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<double> kPermissionPredictionsV2HoldbackChance;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string> kPermissionsPromptSurveyTriggerId;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionsPromptSurveyCustomInvitationTriggerId;

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

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionPromptSurveyPepcPromptPositionFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionPromptSurveyInitialPermissionStatusFilter;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kWebKioskBrowserPermissionsAllowlist;

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kKeyboardLockPromptUIStyle;
#endif
}  // namespace feature_params
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_FEATURES_H_
