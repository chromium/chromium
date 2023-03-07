// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "https_only_mode_metrics.h"

namespace security_interstitials::https_only_mode {

const char kEventHistogram[] = "Security.HttpsFirstMode.NavigationEvent";
const char kNavigationRequestSecurityLevelHistogram[] =
    "Security.NavigationRequestSecurityLevel";

// TODO(crbug.com/1394910): Rename these metrics now that they apply to both
// HTTPS-First Mode and HTTPS Upgrades.
void RecordHttpsFirstModeNavigation(Event event) {
  base::UmaHistogramEnumeration(kEventHistogram, event);
}

void RecordNavigationRequestSecurityLevel(
    NavigationRequestSecurityLevel level) {
  base::UmaHistogramEnumeration(kNavigationRequestSecurityLevelHistogram,
                                level);
}

}  // namespace security_interstitials::https_only_mode
