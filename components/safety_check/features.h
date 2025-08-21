// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFETY_CHECK_FEATURES_H_
#define COMPONENTS_SAFETY_CHECK_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace safety_check::features {

BASE_DECLARE_FEATURE(kSafetyHub);

extern const base::FeatureParam<base::TimeDelta>
    kBackgroundPasswordCheckInterval;

extern const base::FeatureParam<base::TimeDelta> kPasswordCheckOverdueInterval;

extern const base::FeatureParam<int> kPasswordCheckMonWeight;
extern const base::FeatureParam<int> kPasswordCheckTueWeight;
extern const base::FeatureParam<int> kPasswordCheckWedWeight;
extern const base::FeatureParam<int> kPasswordCheckThuWeight;
extern const base::FeatureParam<int> kPasswordCheckFriWeight;
extern const base::FeatureParam<int> kPasswordCheckSatWeight;
extern const base::FeatureParam<int> kPasswordCheckSunWeight;

// TODO(crbug.com/399056993): Remove flags to experiment with engagement scores
// in the notification module.
extern const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsMinEnagementLimit;
extern const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsLowEnagementLimit;

extern const char kPasswordCheckNotificationIntervalName[];
extern const char kRevokedPermissionsNotificationIntervalName[];
extern const char kNotificationPermissionsNotificationIntervalName[];
extern const char kSafeBrowsingNotificationIntervalName[];

extern const base::FeatureParam<base::TimeDelta>
    kPasswordCheckNotificationInterval;

extern const base::FeatureParam<base::TimeDelta>
    kRevokedPermissionsNotificationInterval;

extern const base::FeatureParam<base::TimeDelta>
    kNotificationPermissionsNotificationInterval;

extern const base::FeatureParam<base::TimeDelta>
    kSafeBrowsingNotificationInterval;

}  // namespace safety_check::features

#endif  // COMPONENTS_SAFETY_CHECK_FEATURES_H_
