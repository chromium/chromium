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

// When enabled, use the value of the `service_url` FeatureParam as the url
// for the Web Permission Predictions Service.
const base::Feature kPermissionPredictionServiceUseUrlOverride{
    "kPermissionPredictionServiceUseUrlOverride",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
namespace feature_params {

const base::FeatureParam<bool> kOkButtonBehavesAsAllowAlways(
    &permissions::features::kOneTimeGeolocationPermission,
    "OkButtonBehavesAsAllowAlways",
    true);

const base::FeatureParam<std::string> kPermissionPredictionServiceUrlOverride{
    &permissions::features::kPermissionPredictionServiceUseUrlOverride,
    "service_url", ""};

}  // namespace feature_params
}  // namespace permissions
