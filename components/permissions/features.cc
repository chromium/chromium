// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/features.h"
#include "base/time/time.h"

namespace permissions {
namespace features {

// Enables or disables whether pages with pending permission requests will
// go into back/forward cache.
BASE_FEATURE(kBackForwardCacheUnblockPermissionRequest,
             "BackForwardCacheUnblockPermissionRequest",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Once the user has auto re-authenticated, automatically block subsequent auto
// re-authn prompts within the next 10 minutes.
BASE_FEATURE(kBlockRepeatedAutoReauthnPrompts,
             "BlockRepeatedAutoReauthnPrompts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Once the user declines a notification permission prompt in a WebContents,
// automatically dismiss subsequent prompts in the same WebContents, from any
// origin, until the next user-initiated navigation.
BASE_FEATURE(kBlockRepeatedNotificationPermissionPrompts,
             "BlockRepeatedNotificationPermissionPrompts",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kConfirmationChip,
             "ConfirmationChip",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChipLocationBarIconOverride,
             "ChipLocationIconOverride",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionElement,
             "PermissionElement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationInteractionHistory,
             "NotificationInteractionHistory",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOneTimePermission,
             "OneTimePermission",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Not supported on Android.
BASE_FEATURE(kPermissionChip,
             "PermissionChip",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionQuietChip,
             "PermissionQuietChip",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else

// Enables an experimental permission prompt that uses a chip in the location
// bar.
BASE_FEATURE(kPermissionChip,
             "PermissionChip",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a less prominent permission prompt that uses a chip in the location
// bar. Requires chrome://flags/#quiet-notification-prompts to be enabled.
BASE_FEATURE(kPermissionQuietChip,
             "PermissionQuietChip",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables a faster permission request finalization if it is displayed as a
// quiet chip.
BASE_FEATURE(kFailFastQuietChip,
             "FailFastQuietChip",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionDedicatedCpssSetting,
             "PermissionDedicatedCpssSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)

// When enabled, blocks notifications permission prompt when Chrome doesn't
// have app level Notification permission.
BASE_FEATURE(kBlockNotificationPromptsIfDisabledOnAppLevel,
             "BlockNotificationPromptsIfDisabledOnAppLevel",
             base::FEATURE_ENABLED_BY_DEFAULT);

#else

// Controls whether to trigger showing a HaTS survey, with the given
// `probability` and `trigger_id`. The `probability` parameter is defined and
// handled by the HatsService itself. If the parameter
// `kPermissionsPromptSurveyDisplayTime` is set to `OnPromptResolved` (default),
// the survey is shown immediately after the user has taken the action specified
// in `action_filter` on a permission prompt for the capability specified in
// `request_type_filter`. If, on the other hand, the
// `kPermissionsPromptSurveyDisplayTime` is set to `OnPromptAppearing`, the
// survey is shown when the prompt is first shown to the user. Note, that
// configuring `PermissionAction` does not make sense in that case, since the
// user has not yet taken an action. Therefore, that parameter is ignored in
// that case.
BASE_FEATURE(kPermissionsPromptSurvey,
             "PermissionsPromptSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, permissions grants with a durable session model will have
// an expiration date set. The interpretation of the expiration date
// is not handled by this component, but left to the embedding browser.
BASE_FEATURE(kRecordPermissionExpirationTimestamps,
             "RecordPermissionExpirationTimestamps",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for the mitigation for https://crbug.com/1462709
BASE_FEATURE(kMitigateUnpartitionedWebviewPermissions,
             "MitigateUnpartitionedWebviewPermissions",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, permission grants for Storage Access API will be enabled.
// This includes enabling prompts, a new settings page and page info and
// omnibox integration.
BASE_FEATURE(kPermissionStorageAccessAPI,
             "PermissionStorageAccessAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled "window-management" may be used as an alias for
// "window-placement". Additionally, reverse mappings (i.e. enum to string) will
// default to the new alias.
BASE_FEATURE(kWindowManagementPermissionAlias,
             "WindowManagementPermissionAlias",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables disallowing MIDI permission by default.
BASE_FEATURE(kBlockMidiByDefault,
             "BlockMidiByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
namespace feature_params {

const base::FeatureParam<bool> kUseStrongerPromptLanguage{
    &features::kOneTimePermission, "use_stronger_prompt_language", false};

const base::FeatureParam<base::TimeDelta> kOneTimePermissionTimeout{
    &features::kOneTimePermission, "one_time_permission_timeout",
    base::Minutes(5)};

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
// the user has interacted with a permission prompt. Multiple values can be
// configured by providing a comma separated list. If this is done, a
// corresponding probability_vector and request_type_filter of equal length must
// be configured. If this is done, each trigger_id applies to the request type
// at the corresponding position in the request_type_filter and has probability
// of probability p * hats_p of triggering, where p is the probability at the
// corresponding position in the probability_vector and hats_p is the
// probability configured for the HaTS survey.
const base::FeatureParam<std::string> kPermissionsPromptSurveyTriggerId{
    &permissions::features::kPermissionsPromptSurvey, "trigger_id", ""};

// If multiple trigger ids are configured, the trigger id at position p only
// triggers for the request type at position p of the request type filter,
// and calls the HaTS service with the probability at position p in the
// probability vector. The HaTS service also has a feature parameter called
// probability. The probability vector is a secondary probability to
// distribute surveys among the multiple triggers, while the HaTS service
// probability is the probability of triggering overall.
const base::FeatureParam<std::string> kProbabilityVector{
    &permissions::features::kPermissionsPromptSurvey, "probability_vector",
    "1.0"};

// Specifies the type of permission request for which the prompt HaTS
// survey is triggered (as long as other filters are also satisfied). Valid
// values are the return values of `GetPermissionRequestString`. An empty value
// will result in all request types matching (no filtering on request types).
// Use caution when configuring multiple values. Each study can only specify one
// probability value. Some request types have a vastly different number of
// occurrences then others, which likely makes them a bad match for combining
// them in the same study.
const base::FeatureParam<std::string> kPermissionsPromptSurveyRequestTypeFilter{
    &permissions::features::kPermissionsPromptSurvey, "request_type_filter",
    ""};

// A survey can either be triggered when the prompt is shown or afterwards.
// Valid configuration values are `OnPromptAppearing` and `OnPromptResolved`.
const base::FeatureParam<std::string> kPermissionsPromptSurveyDisplayTime{
    &permissions::features::kPermissionsPromptSurvey, "survey_display_time",
    ""};

// Specifies the actions for which the prompt HaTS survey is triggered (as
// long as other filters are also satisfied). Multiple values can be configured
// by providing a comma separated list. Valid values are those listed in
// PermissionUmaUtil::GetPermissionActionString. An empty value will result in
// all actions matching (no filtering on actions). Note, that this parameter is
// ignored if `SurveyDisplayTime` is set to `OnPromptAppearing`.
const base::FeatureParam<std::string> kPermissionsPromptSurveyActionFilter{
    &permissions::features::kPermissionsPromptSurvey, "action_filter", ""};

// Specifies whether the prompt HaTS survey is triggered for permission
// requests with or without user gesture (as long as other filters are also
// satisfied). Valid values are 'true' and 'false'. An empty value or
// 'true,false' will result in all requests matching (no filtering on user
// gesture).
const base::FeatureParam<std::string> kPermissionsPromptSurveyHadGestureFilter{
    &permissions::features::kPermissionsPromptSurvey, "had_gesture_filter", ""};

// Specifies the prompt disposition(s) for which the prompt HaTS
// survey is triggered (as long as other filters are also satisfied). Multiple
// values can be configured by providing a comma separated list. Valid values
// are those listed in PermissionUmaUtil::GetPromptDispositionString. An empty
// value will result in all prompt dispositions matching (no filtering on prompt
// dispositions).
const base::FeatureParam<std::string>
    kPermissionsPromptSurveyPromptDispositionFilter{
        &permissions::features::kPermissionsPromptSurvey,
        "prompt_disposition_filter", ""};

// Specifies the prompt disposition reason(s) for which the prompt HaTS
// survey is triggered (as long as other filters are also satisfied). Multiple
// values can be configured by providing a comma separated list. Valid values
// are those listed in PermissionUmaUtil::GetPromptDispositionReasonString. An
// empty value will result in all prompt disposition reasons matching (no
// filtering on prompt disposition reasons).
const base::FeatureParam<std::string>
    kPermissionsPromptSurveyPromptDispositionReasonFilter{
        &permissions::features::kPermissionsPromptSurvey,
        "prompt_disposition_reason_filter", ""};

// Specifies the browser channel(s) for which the prompt HaTS survey is
// triggered (as long as other filters are also satisfied). Multiple values can
// be configured by providing a comma separated list. Valid values are those
// listed in version_info::GetChannelString. An empty value will result in all
// channels matching (no filtering on channels within HaTS). This filter allows
// restriction to specific channels (typically to stable). Inform Finch team
// when configuring this filter, as it will effectively disable this feature on
// certain channels.
const base::FeatureParam<std::string>
    kPermissionPromptSurveyReleaseChannelFilter{
        &permissions::features::kPermissionsPromptSurvey,
        "release_channel_filter", ""};

// Some prompts stay open for a long time. This parameter allows specifying an
// upper bound on how long a prompt that has been ignored can have been
// showing and still trigger a survey if all other filters match. Prompts that
// have been open longer before being ignored do not trigger a survey anymore.
const base::FeatureParam<base::TimeDelta>
    kPermissionPromptSurveyIgnoredPromptsMaximumAge{
        &permissions::features::kPermissionsPromptSurvey,
        "ignored_prompts_maximum_age", base::Minutes(10)};

// We count the number of one time permission prompt impressions that a user has
// seen. This parameter specifies the buckets to which a user needs to belong to
// in order for a HaTS survey to be triggered. Multiple values can be configured
// by providing a comma separated list. Valid values are the return values of
// `PermissionUtil::GetOneTimePromptsDecidedBucketString`. An empty value will
// result in all buckets matching (no filtering).
const base::FeatureParam<std::string>
    kPermissionPromptSurveyOneTimePromptsDecidedBucket{
        &permissions::features::kPermissionsPromptSurvey,
        "one_time_prompts_decided_bucket", ""};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace feature_params
}  // namespace permissions
