// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service_features.h"

namespace omnibox {

// If enabled, uses the server response for AIM eligibility for all locales.
BASE_FEATURE(kAimServerEligibilityEnabled, base::FEATURE_DISABLED_BY_DEFAULT);
// If enabled, uses the server response for AIM eligibility for English locales.
// Has no effect if kAimServerEligibilityEnabled is enabled.
BASE_FEATURE(kAimServerEligibilityEnabledEn, base::FEATURE_DISABLED_BY_DEFAULT);
// If enabled, notifies AIM eligibility changes.
BASE_FEATURE(kAimServerEligibilityChangedNotification,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace omnibox
