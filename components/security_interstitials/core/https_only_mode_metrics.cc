// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "https_only_mode_metrics.h"

namespace security_interstitials::https_only_mode {

const char kEventHistogram[] = "Security.HttpsFirstMode.NavigationEvent";
const char kEventHistogramWithEngagementHeuristic[] =
    "Security.HttpsFirstModeWithEngagementHeuristic.NavigationEvent";

const char kNavigationRequestSecurityLevelHistogram[] =
    "Security.NavigationRequestSecurityLevel";

// TODO(crbug.com/1394910): Rename these metrics now that they apply to both
// HTTPS-First Mode and HTTPS Upgrades.
void RecordHttpsFirstModeNavigation(
    Event event,
    const HttpInterstitialState& interstitial_state) {
  base::UmaHistogramEnumeration(kEventHistogram, event);

  if (!interstitial_state.enabled_by_pref &&
      interstitial_state.enabled_by_engagement_heuristic) {
    // Only record the engagement heuristic histogram if HTTPS-First Mode wasn't
    // enabled by the UI setting.
    base::UmaHistogramEnumeration(kEventHistogramWithEngagementHeuristic,
                                  event);
  }
}

void RecordNavigationRequestSecurityLevel(
    NavigationRequestSecurityLevel level) {
  base::UmaHistogramEnumeration(kNavigationRequestSecurityLevelHistogram,
                                level);
}

}  // namespace security_interstitials::https_only_mode
