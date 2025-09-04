// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_FEATURES_H_
#define COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_FEATURES_H_

#include "base/feature_list.h"

namespace omnibox {

// If enabled, uses the server response for AIM eligibility for all locales.
BASE_DECLARE_FEATURE(kAimServerEligibilityEnabled);
// If enabled, uses the server response for AIM eligibility for English locales.
// Has no effect if kAimServerEligibilityEnabled is enabled.
BASE_DECLARE_FEATURE(kAimServerEligibilityEnabledEn);
// If enabled, notifies AIM eligibility changes.
BASE_DECLARE_FEATURE(kAimServerEligibilityChangedNotification);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_FEATURES_H_
