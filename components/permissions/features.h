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
BASE_DECLARE_FEATURE(kBackForwardCacheUnblockPermissionRequest);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockPromptsIfDismissedOften);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockPromptsIfIgnoredOften);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockRepeatedAutoReauthnPrompts);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockRepeatedNotificationPermissionPrompts);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kOneTimePermission);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kFailFastQuietChip);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kKeyboardAndPointerLockPrompt);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionElementPromptPositioning);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionOnDeviceNotificationPredictions);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionOnDeviceGeolocationPredictions);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionPredictionsV2);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionPredictionsV3);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionsPromptSurvey);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kAllowMultipleOriginsForWebKioskPermissions);

#if BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kBlockNotificationPromptsIfDisabledOnAppLevel);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kPermissionDedicatedCpssSettingAndroid);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kRecordPermissionExpirationTimestamps);

#else

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kRecordChooserPermissionLastVisitedTimestamps);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kMitigateUnpartitionedWebviewPermissions);

#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kShowRelatedWebsiteSetsPermissionGrants);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kCpssQuietChipTextUpdate);

COMPONENT_EXPORT(PERMISSIONS_COMMON)
BASE_DECLARE_FEATURE(kCpssUseTfliteSignatureRunner);

}  // namespace features
namespace feature_params {

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kUseStrongerPromptLanguage;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kUseWhileVisitingLanguage;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kShowAllowAlwaysAsFirstButton;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<base::TimeDelta> kOneTimePermissionTimeout;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<base::TimeDelta> kOneTimePermissionLongTimeout;

enum class PermissionElementPromptPosition {
  kWindowMiddle,
  kNearElement,
  kLegacyPrompt,
};

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<PermissionElementPromptPosition>
    kPermissionElementPromptPositioningParam;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<double>
    kPermissionOnDeviceGeolocationPredictionsHoldbackChance;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<double>
    kPermissionOnDeviceNotificationPredictionsHoldbackChance;

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

}  // namespace feature_params
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_FEATURES_H_
