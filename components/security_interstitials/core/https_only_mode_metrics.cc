// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace security_interstitials::https_only_mode {

namespace {

InterstitialReason GetInterstitialReason(
    const HttpInterstitialState& interstitial_state) {
  // This should follow the order in
  // PopulateHttpsOnlyModeStringsForBlockingPage() in
  // https_only_mode_ui_utils.cc.
  if (interstitial_state.enabled_by_advanced_protection) {
    return InterstitialReason::kAdvancedProtection;
  }
  if (interstitial_state.enabled_by_engagement_heuristic) {
    return InterstitialReason::kSiteEngagementHeuristic;
  }
  if (interstitial_state.enabled_by_typically_secure_browsing) {
    return InterstitialReason::kTypicallySecureUserHeuristic;
  }
  if (interstitial_state.enabled_by_pref) {
    return InterstitialReason::kPref;
  }
  if (interstitial_state.enabled_by_incognito) {
    return InterstitialReason::kIncognito;
  }
  if (interstitial_state.enabled_in_balanced_mode) {
    return InterstitialReason::kBalanced;
  }
  return InterstitialReason::kUnknown;
}

}  // namespace

const char kEventHistogram[] = "Security.HttpsFirstMode.NavigationEvent";
const char kEventHistogramWithEngagementHeuristic[] =
    "Security.HttpsFirstModeWithEngagementHeuristic.NavigationEvent";

const char kNavigationRequestSecurityLevelHistogram[] =
    "Security.NavigationRequestSecurityLevel";

const char kSiteEngagementHeuristicStateHistogram[] =
    "Security.HttpsFirstModeWithEngagementHeuristic.State";

const char kSiteEngagementHeuristicHostCountHistogram[] =
    "Security.HttpsFirstModeWithEngagementHeuristic.HostCount";

const char kSiteEngagementHeuristicAccumulatedHostCountHistogram[] =
    "Security.HttpsFirstModeWithEngagementHeuristic.AccumulatedHostCount";

const char kSiteEngagementHeuristicEnforcementDurationHistogram[] =
    "Security.HttpsFirstModeWithEngagementHeuristic.Duration";

const char kInterstitialReasonHistogram[] =
    "Security.HttpsFirstMode.InterstitialReason";

// TODO(crbug.com/40248833): Rename these metrics now that they apply to both
// HTTPS-First Mode and HTTPS Upgrades.
void RecordHttpsFirstModeNavigation(
    Event event,
    const HttpInterstitialState& interstitial_state) {
  base::UmaHistogramEnumeration(kEventHistogram, event);

  if (!interstitial_state.enabled_by_pref &&
      !interstitial_state.enabled_in_balanced_mode &&
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

void RecordSiteEngagementHeuristicState(SiteEngagementHeuristicState state) {
  base::UmaHistogramEnumeration(kSiteEngagementHeuristicStateHistogram, state);
}

void RecordSiteEngagementHeuristicCurrentHostCounts(size_t current_count,
                                                    size_t accumulated_count) {
  base::UmaHistogramCounts1000(kSiteEngagementHeuristicHostCountHistogram,
                               current_count);
  base::UmaHistogramCounts1000(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, accumulated_count);
}

void RecordSiteEngagementHeuristicEnforcementDuration(
    base::TimeDelta enforcement_duration) {
  base::UmaHistogramTimes(kSiteEngagementHeuristicEnforcementDurationHistogram,
                          enforcement_duration);
}

void RecordInterstitialReason(const HttpInterstitialState& interstitial_state) {
  base::UmaHistogramEnumeration(kInterstitialReasonHistogram,
                                GetInterstitialReason(interstitial_state));
}

}  // namespace security_interstitials::https_only_mode
