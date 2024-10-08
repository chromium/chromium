// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/features.h"

#include "base/feature_list.h"
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

BASE_FEATURE(kOneTimePermission,
             "OneTimePermission",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a faster permission request finalization if it is displayed as a
// quiet chip.
BASE_FEATURE(kFailFastQuietChip,
             "FailFastQuietChip",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kKeyboardAndPointerLockPrompt,
             "KeyboardAndPointerLockPrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

// Enables different positioning of the permission dialog, so that it's placed
// near the permission element, if possible.
// This feature should be enabled with blink::features::kPermissionElement.
BASE_FEATURE(kPermissionElementPromptPositioning,
             "PermissionElementPromptPositioning",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionOnDeviceNotificationPredictions,
             "PermissionOnDeviceNotificationPredictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionOnDeviceGeolocationPredictions,
             "PermissionOnDeviceGeolocationPredictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionPredictionsV2,
             "PermissionPredictionsV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionPredictionsV3,
             "PermissionPredictionsV3",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// When enabled, use the value of the `allowlist_urls` FeatureParam as the
// list of origins which would be allowed to access browser permission and
// device attribute API for a web kiosk session.
BASE_FEATURE(kAllowMultipleOriginsForWebKioskPermissions,
             "AllowMultipleOriginsForWebKioskPermissions",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)

// When enabled, blocks notifications permission prompt when Chrome doesn't
// have app level Notification permission.
BASE_FEATURE(kBlockNotificationPromptsIfDisabledOnAppLevel,
             "BlockNotificationPromptsIfDisabledOnAppLevel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionDedicatedCpssSettingAndroid,
             "PermissionDedicatedCpssSettingAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, permissions grants with a durable session model will have
// an expiration date set.
BASE_FEATURE(kRecordPermissionExpirationTimestamps,
             "RecordPermissionExpirationTimestamps",
             base::FEATURE_ENABLED_BY_DEFAULT);

#else

// When enabled, chooser permissions grants will have a last visited timestamp
// date set. The timestamp will be later used to auto-revoke the permission,
// if eligible.
BASE_FEATURE(kRecordChooserPermissionLastVisitedTimestamps,
             "RecordChooserPermissionLastVisitedTimestamps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for the mitigation for https://crbug.com/1462709
BASE_FEATURE(kMitigateUnpartitionedWebviewPermissions,
             "MitigateUnpartitionedWebviewPermissions",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, blocks condition to exclude auto granted permissions for
// storage access exceptions. This will allow RWS permission grants to be
// visible in the Embedded content settings page.
BASE_FEATURE(kShowRelatedWebsiteSetsPermissionGrants,
             "ShowRelatedWebsiteSetsPermissionGrants",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Quiet prompts triggered by CPSS will have "Get Notifications?"
// as the the chip text instead of the usual "Notifications Blocked".
BASE_FEATURE(kCpssQuietChipTextUpdate,
             "CpssQuietChipTextUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCpssUseTfliteSignatureRunner,
             "CpssUseTfliteSignatureRunner",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features
namespace feature_params {

const base::FeatureParam<bool> kUseStrongerPromptLanguage{
    &features::kOneTimePermission, "use_stronger_prompt_language", false};

const base::FeatureParam<bool> kUseWhileVisitingLanguage{
    &features::kOneTimePermission, "use_while_visiting_language", false};

const base::FeatureParam<bool> kShowAllowAlwaysAsFirstButton{
    &features::kOneTimePermission, "show_allow_always_as_first_button", false};

const base::FeatureParam<base::TimeDelta> kOneTimePermissionTimeout{
    &features::kOneTimePermission, "one_time_permission_timeout",
    base::Minutes(5)};

const base::FeatureParam<base::TimeDelta> kOneTimePermissionLongTimeout{
    &features::kOneTimePermission, "one_time_permission_long_timeout",
    base::Hours(16)};

const base::FeatureParam<PermissionElementPromptPosition>::Option
    kPromptPositioningOptions[] = {
        {PermissionElementPromptPosition::kWindowMiddle, "window_middle"},
        {PermissionElementPromptPosition::kNearElement, "near_element"},
        {PermissionElementPromptPosition::kLegacyPrompt, "legacy_prompt"}};

const base::FeatureParam<PermissionElementPromptPosition>
    kPermissionElementPromptPositioningParam = {
        &features::kPermissionElementPromptPositioning,
        "PermissionElementPromptPositioningParam",
        PermissionElementPromptPosition::kWindowMiddle,
        &kPromptPositioningOptions};

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

const base::FeatureParam<double> kPermissionPredictionsV2HoldbackChance(
    &features::kPermissionPredictionsV2,
    "holdback_chance",
    0.3);

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

// WARNING: This parameter is intended only for a one-off A/B experiment on
// Clank (see crbug.com/1502780) and will be removed thereafter.
// The experiment is active iff |experimental_custom_invitation_arm_trigger_id|
// is configured. If it is active, a coin flip determines whether the generic or
// a custom invitation is shown. These two cases will use distinct trigger IDs
// in order to properly convey the survey context to the user in both cases. The
// parameter specifies the alternate set of trigger IDs for the HaTS surveys
// that should be shown after a customized invitation was shown. The triggerIds
// configured in |trigger_id| are used if a generic invitation was shown. The
// configuration of |experimental_custom_invitation_arm_trigger_id| is analogous
// to that of |trigger_id|. Custom invitations are hardcoded and only supported
// for the request types geolocation, camera, and microphone.
const base::FeatureParam<std::string>
    kPermissionsPromptSurveyCustomInvitationTriggerId{
        &permissions::features::kPermissionsPromptSurvey,
        "experimental_custom_invitation_arm_trigger_id", ""};

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

// This parameter specifies which prompt position should have been used for a
// HaTS survery to be allowed to trigger. It only applies to permission element
// prompts (PEPC). Valid values are the values in the
// |kPromptPositioningOptions| array. Multiple values can be configured by
// providing a comma separated list and an empty value means no filtering (all
// allowed).
const base::FeatureParam<std::string>
    kPermissionPromptSurveyPepcPromptPositionFilter{
        &permissions::features::kPermissionsPromptSurvey,
        "pepc_prompt_position_filter", ""};

// This parameter specifies what the initial permission status was before a
// permission prompt was displayed. It's only relevant to permission element
// prompts (PEPC), since other prompts will always report "ask". Valid values
// are the values returned by |content_settings::ContentSettingToString| util
// function. Multiple values can be configured by providing a comma separated
// list and an empty value means no filtering (all allowed).
const base::FeatureParam<std::string>
    kPermissionPromptSurveyInitialPermissionStatusFilter{
        &permissions::features::kPermissionsPromptSurvey,
        "initial_permission_status_filter", ""};

// Comma separated url patterns which should be allowed for accessing web kiosk
// browser permissions and device attributes API. If left empty no URL patterns
// will be allowed.
const base::FeatureParam<std::string> kWebKioskBrowserPermissionsAllowlist{
    &permissions::features::kAllowMultipleOriginsForWebKioskPermissions,
    "allowlist_urls", ""};

}  // namespace feature_params
}  // namespace permissions
