// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/features.h"

namespace permissions {
namespace features {

// Enables or disables whether permission prompts are automatically blocked
// after the user has explicitly dismissed them too many times.
BASE_FEATURE(kBlockPromptsIfDismissedOften,
             "BlockPromptsIfDismissedOften",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables whether permission prompts are automatically blocked
// after the user has ignored them too many times.
BASE_FEATURE(kBlockPromptsIfIgnoredOften,
             "BlockPromptsIfIgnoredOften",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Once the user declines a notification permission prompt in a WebContents,
// automatically dismiss subsequent prompts in the same WebContents, from any
// origin, until the next user-initiated navigation.
BASE_FEATURE(kBlockRepeatedNotificationPermissionPrompts,
             "BlockRepeatedNotificationPermissionPrompts",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kConfirmationChip,
             "ConfirmationChip",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChipLocationBarIconOverride,
             "ChipLocationIconOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationInteractionHistory,
             "NotificationInteractionHistory",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOneTimeGeolocationPermission,
             "OneTimeGeolocationPermission",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an experimental permission prompt that uses a chip in the location
// bar.
BASE_FEATURE(kPermissionChip,
             "PermissionChip",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Not supported on Android.
BASE_FEATURE(kPermissionQuietChip,
             "PermissionQuietChip",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else

// Enables a less prominent permission prompt that uses a chip in the location
// bar. Requires chrome://flags/#quiet-notification-prompts to be enabled.
BASE_FEATURE(kPermissionQuietChip,
             "PermissionQuietChip",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPermissionChipAutoDismiss,
             "PermissionChipAutoDismiss",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a faster permission request finalization if it is displayed as a
// quiet chip.
BASE_FEATURE(kFailFastQuietChip,
             "FailFastQuietChip",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kPermissionChipAutoDismissDelay{
    &kPermissionChipAutoDismiss, "delay_ms", 6000};

// When enabled, use the value of the `service_url` FeatureParam as the url
// for the Web Permission Predictions Service.
BASE_FEATURE(kPermissionPredictionServiceUseUrlOverride,
             "kPermissionPredictionServiceUseUrlOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionOnDeviceNotificationPredictions,
             "PermissionOnDeviceNotificationPredictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionOnDeviceGeolocationPredictions,
             "PermissionOnDeviceGeolocationPredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)

// When enabled, blocks notifications permission prompt when Chrome doesn't
// have app level Notification permission.
BASE_FEATURE(kBlockNotificationPromptsIfDisabledOnAppLevel,
             "BlockNotificationPromptsIfDisabledOnAppLevel",
             base::FEATURE_ENABLED_BY_DEFAULT);

#else

// Controls whether to trigger showing a HaTS survey, with the given
// `probability` and `trigger_id`, immediately after the user has taken the
// action specified in `action_filter` on a permission prompt for the capability
// specified in `request_type_filter`. All of the above-mentioned params are
// required and should be coming from field trial params of the same name. The
// `probability` parameter is an odd-one out and is defined and handled by the
// HatsService itself.
BASE_FEATURE(kPermissionsPostPromptSurvey,
             "PermissionsPostPromptSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, permissions grants with a durable session model will have
// an expiration date set. The interpretation of the expiration date
// is not handled by this component, but left to the embedding browser.
BASE_FEATURE(kRecordPermissionExpirationTimestamps,
             "RecordPermissionExpirationTimestamps",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
namespace feature_params {

const base::FeatureParam<bool> kOkButtonBehavesAsAllowAlways(
    &permissions::features::kOneTimeGeolocationPermission,
    "OkButtonBehavesAsAllowAlways",
    true);

const base::FeatureParam<std::string> kPermissionPredictionServiceUrlOverride{
    &permissions::features::kPermissionPredictionServiceUseUrlOverride,
    "service_url", ""};

const base::FeatureParam<double>
    kPermissionOnDeviceGeolocationPredictionsHoldbackChance(
        &features::kPermissionOnDeviceGeolocationPredictions,
        "holdback_chance",
        0.3);

const base::FeatureParam<double>
    kPermissionOnDeviceNotificationPredictionsHoldbackChance(
        &features::kPermissionOnDeviceNotificationPredictions,
        "holdback_chance",
        0.2);

#if !BUILDFLAG(IS_ANDROID)
// Specifies the `trigger_id` of the HaTS survey to trigger immediately after
// the user has interacted with a permission prompt.
const base::FeatureParam<std::string> kPermissionsPostPromptSurveyTriggerId{
    &permissions::features::kPermissionsPostPromptSurvey, "trigger_id", ""};

// Specifies the type of permission request for which the post-prompt HaTS
// survey is triggered. For any given user, there is a single request type for
// which they may see a survey. Valid values are the return values of
// `GetPermissionRequestString`. An invalid or empty value will result in the
// user not seeing any post-prompt survey.
const base::FeatureParam<std::string>
    kPermissionsPostPromptSurveyRequestTypeFilter{
        &permissions::features::kPermissionsPostPromptSurvey,
        "request_type_filter", ""};

// Specifies the action for which the post-prompt HaTS survey is triggered. For
// any given user, there is a single permission action for which they may see a
// survey, of those listed in RetuPermissionUmaUtil::GetPermissionActionString.
// An invalid or empty value will result in the user not seeing any post-prompt
// survey.
const base::FeatureParam<std::string> kPermissionsPostPromptSurveyActionFilter{
    &permissions::features::kPermissionsPostPromptSurvey, "action_filter", ""};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace feature_params
}  // namespace permissions
