// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service_features.h"

#include "base/metrics/field_trial_params.h"

namespace omnibox {

BASE_FEATURE(kAimEnabled, "AimEnabled", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilityEnabled,
             "AimServerEligibilityEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilityEnabledEn,
             "AimServerEligibilityEnabledEn",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilityChangedNotification,
             "AimServerEligibilityChangedNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerRequestOnStartupEnabled,
             "AimServerRequestOnStartupEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerRequestOnIdentityChangeEnabled,
             "AimServerRequestOnIdentityChangeEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<bool> kRequestOnCookieJarChanges{
    &kAimServerRequestOnIdentityChangeEnabled, "request_on_cookie_jar_changes",
    false};
const base::FeatureParam<bool> kRequestOnPrimaryAccountChanges{
    &kAimServerRequestOnIdentityChangeEnabled,
    "request_on_primary_account_changes", true};

}  // namespace omnibox
