// Copyright 2020 The Chromium Authors. All rights reserved.
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
extern const base::Feature kBlockPromptsIfDismissedOften;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kBlockPromptsIfIgnoredOften;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kBlockRepeatedNotificationPermissionPrompts;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kOneTimeGeolocationPermission;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kPermissionChip;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kPermissionQuietChip;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kPermissionChipAutoDismiss;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<int> kPermissionChipAutoDismissDelay;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kPermissionChipGestureSensitive;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kPermissionChipRequestTypeSensitive;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kPermissionChipIsProminentStyle;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kPermissionPredictionServiceUseUrlOverride;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kRevisedOriginHandling;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::Feature kRevertDSEAutomaticPermissions;
#endif  // defined(OS_ANDROID)

}  // namespace features
namespace feature_params {

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kOkButtonBehavesAsAllowAlways;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<std::string>
    kPermissionPredictionServiceUrlOverride;

COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::FeatureParam<bool> kPermissionPredictionServiceUseJson;

}  // namespace feature_params
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_FEATURES_H_
