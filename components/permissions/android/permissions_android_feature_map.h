// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSIONS_ANDROID_FEATURE_MAP_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSIONS_ANDROID_FEATURE_MAP_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace permissions {
// Alphabetical:
BASE_DECLARE_FEATURE(kAndroidCancelPermissionPromptOnTouchOutside);

BASE_DECLARE_FEATURE(kPermissionsAndroidClapperLoud);
extern const base::FeatureParam<base::TimeDelta> kClapperLoudTimeout;

BASE_DECLARE_FEATURE(kPermissionsAndroidClapperQuiet);
BASE_DECLARE_FEATURE(kPermissionsGestureGatedPrompts);
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSIONS_ANDROID_FEATURE_MAP_H_
