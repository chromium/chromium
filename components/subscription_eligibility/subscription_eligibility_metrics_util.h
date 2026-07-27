// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_UTIL_H_
#define COMPONENTS_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_UTIL_H_

#include <cstdint>
#include <set>
#include <string>

namespace subscription_eligibility {

// LINT.IfChange(AiSubscriptionTierStatus)
enum class AiSubscriptionTierStatus {
  kValueNotSet = 0,
  // No profiles have an AI subscription tier.
  kNoProfilesSubscribed = 1,
  // Some profiles have an AI subscription tier, some do not.
  kSomeProfilesSubscribed = 2,
  // All profiles have an AI subscription tier but are different.
  kAllProfilesSubscribedButDifferentTiers = 3,
  // All profiles have the same AI subscription tier but for a tier not known to
  // this browser.
  kAllProfilesSubscribedForUnknownTier = 4,
  // All profiles subscribed for tier=1 AI subscription tier.
  kAllProfilesAtTierEquals1 = 5,
  // All profiles subscribed for tier=2 AI subscription tier.
  kAllProfilesAtTierEquals2 = 6,
  // All profiles subscribed for tier=3 AI subscription tier.
  kAllProfilesAtTierEquals3 = 7,

  // Values must not be deleted or repurposed. Must be kept in sync with
  // SubscriptionEligibilityAiSubscriptionTierStatus in others.enums.xml. Please
  // also update the kSubscriptionEligibilityAiSubscriptionTierStatus in
  // go/internal-pipeline-ai-subsciption-status.

  // New values above this line.
  kMaxValue = kAllProfilesAtTierEquals3,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:SubscriptionEligibilityAiSubscriptionTierStatus)

// Histogram name for AI subscription tier status.
extern const char kAiSubscriptionTierStatusHistogramName[];

// Synthetic trial name for AI subscription tier.
extern const char kAiSubscriptionTierSyntheticTrialName[];

// Returns the synthetic field trial group name for the given status.
std::string GetSyntheticTrialGroupName(AiSubscriptionTierStatus status);

// Computes the AiSubscriptionTierStatus from a set of subscription tiers
// present across all profiles.
AiSubscriptionTierStatus ComputeAiSubscriptionTierStatus(
    const std::set<int32_t>& subscription_tiers);

}  // namespace subscription_eligibility

#endif  // COMPONENTS_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_UTIL_H_
