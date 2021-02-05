// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_FEATURES_H_
#define COMPONENTS_PERMISSIONS_FEATURES_H_

#include "base/feature_list.h"

namespace permissions {
namespace features {

extern const base::Feature kBlockPromptsIfDismissedOften;
extern const base::Feature kBlockPromptsIfIgnoredOften;
extern const base::Feature kBlockRepeatedNotificationPermissionPrompts;
extern const base::Feature kOneTimeGeolocationPermission;
extern const base::Feature kPermissionChip;
extern const base::Feature kPermissionPredictionServiceUseUrlOverride;

}  // namespace features
namespace feature_params {

extern const base::FeatureParam<bool> kOkButtonBehavesAsAllowAlways;
extern const base::FeatureParam<std::string>
    kPermissionPredictionServiceUrlOverride;
extern const base::FeatureParam<bool> kPermissionPredictionServiceUseJson;

}  // namespace feature_params
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_FEATURES_H_
