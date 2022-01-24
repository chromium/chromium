// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/features.h"

namespace permissions {
namespace features {

// Enables or disables whether permission prompts are automatically blocked
// after the user has explicitly dismissed them too many times.
const base::Feature kBlockPromptsIfDismissedOften{
    "BlockPromptsIfDismissedOften", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables whether permission prompts are automatically blocked
// after the user has ignored them too many times.
const base::Feature kBlockPromptsIfIgnoredOften{
    "BlockPromptsIfIgnoredOften", base::FEATURE_ENABLED_BY_DEFAULT};

// Once the user declines a notification permission prompt in a WebContents,
// automatically dismiss subsequent prompts in the same WebContents, from any
// origin, until the next user-initiated navigation.
const base::Feature kBlockRepeatedNotificationPermissionPrompts{
    "BlockRepeatedNotificationPermissionPrompts",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOneTimeGeolocationPermission{
    "OneTimeGeolocationPermission", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables an experimental permission prompt that uses a chip in the location
// bar.
const base::Feature kPermissionChip{"PermissionChip",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables an experimental less prominent permission prompt that uses a chip in
// the location bar. Requires chrome://flags/#quiet-notification-prompts to be
// enabled.
const base::Feature kPermissionQuietChip{"PermissionQuietChip",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPermissionChipAutoDismiss{
    "PermissionChipAutoDismiss", base::FEATURE_ENABLED_BY_DEFAULT};
const base::FeatureParam<int> kPermissionChipAutoDismissDelay{
    &kPermissionChipAutoDismiss, "delay_ms", 6000};

// When kPermissionChip (above) is enabled, controls whether or not the
// permission chip should be more prominent when the request is associated with
// a gesture. Does nothing when kPermissionChip is disabled.
const base::Feature kPermissionChipGestureSensitive{
    "PermissionChipGestureSensitive", base::FEATURE_DISABLED_BY_DEFAULT};

// When kPermissionChip (above) is enabled, controls whether or not the
// permission chip should be more or less prominent depending on the request
// type. Does nothing when kPermissionChip is disabled.
const base::Feature kPermissionChipRequestTypeSensitive{
    "PermissionChipRequestTypeSensitive", base::FEATURE_DISABLED_BY_DEFAULT};

// When kPermissionChip (above) is enabled, controls whether or not the
// permission chip should be shown in the prominent style (white on blue) or in
// the secondary style (blue on white).
const base::Feature kPermissionChipIsProminentStyle{
    "PermissionChipIsProminentStyle", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, use the value of the `service_url` FeatureParam as the url
// for the Web Permission Predictions Service.
const base::Feature kPermissionPredictionServiceUseUrlOverride{
    "kPermissionPredictionServiceUseUrlOverride",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, permission verification and requesting is done on
// RFH->GetLastCommittedOrigin() instead of RFH->GetLastCommittedURL().
const base::Feature kRevisedOriginHandling{"PermissionsRevisedOriginHandling",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// When enabled, the Default Search Engine does not automatically receive the
// "geolocation" and "notifications" permissions. DSE only applies to Android.
const base::Feature kRevertDSEAutomaticPermissions{
    "RevertDSEAutomaticPermissions", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

}  // namespace features
namespace feature_params {

const base::FeatureParam<bool> kOkButtonBehavesAsAllowAlways(
    &permissions::features::kOneTimeGeolocationPermission,
    "OkButtonBehavesAsAllowAlways",
    true);

const base::FeatureParam<std::string> kPermissionPredictionServiceUrlOverride{
    &permissions::features::kPermissionPredictionServiceUseUrlOverride,
    "service_url", ""};

const base::FeatureParam<bool> kPermissionPredictionServiceUseJson{
    &permissions::features::kPermissionPredictionServiceUseUrlOverride,
    "service_use_json", false};

}  // namespace feature_params
}  // namespace permissions
