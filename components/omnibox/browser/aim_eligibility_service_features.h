// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_FEATURES_H_
#define COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox {

// If disabled, AIM is completely turned off (kill switch).
BASE_DECLARE_FEATURE(kAimEnabled);

// If enabled, uses the server response for AIM eligibility for all locales.
BASE_DECLARE_FEATURE(kAimServerEligibilityEnabled);

// If enabled, uses a custom retry policy for the server request.
BASE_DECLARE_FEATURE(kAimServerEligibilityCustomRetryPolicyEnabled);

// If enabled, AIM eligibility is obtained for the primary account.
BASE_DECLARE_FEATURE(kAimServerEligibilityForPrimaryAccountEnabled);

// If enabled, makes a server request on service startup.
BASE_DECLARE_FEATURE(kAimServerRequestOnStartupEnabled);

// If enabled, delays the startup server request until the network is available.
BASE_DECLARE_FEATURE(kAimStartupRequestDelayedUntilNetworkAvailableEnabled);

// If enabled, makes a server request on identity changes.
BASE_DECLARE_FEATURE(kAimServerRequestOnIdentityChangeEnabled);
// Parameters that control whether to make a server request on cookie jar or
// primary account changes. Only one of these should be true.
extern const base::FeatureParam<bool> kRequestOnCookieJarChanges;
extern const base::FeatureParam<bool> kRequestOnPrimaryAccountChanges;

// If enabled, all composebox context menus will use the `SearchboxConfig` aka
// PEC API response in the AIM eligibility response.
BASE_DECLARE_FEATURE(kAimUsePecApi);

// Configures whether to include the client locale in the AIM eligibility
// request.
BASE_DECLARE_FEATURE(kAimServerEligibilityIncludeClientLocale);

enum class AimServerEligibilityIncludeClientLocaleMode {
  kLegacyGet,
  kGetWithLocale,
  kPostWithProto,
};

extern const base::FeatureParam<AimServerEligibilityIncludeClientLocaleMode>
    kAimServerEligibilityIncludeClientLocaleMode;

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_FEATURES_H_
