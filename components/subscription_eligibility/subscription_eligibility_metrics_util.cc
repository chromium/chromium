// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/subscription_eligibility_metrics_util.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"

namespace subscription_eligibility {

const char kAiSubscriptionTierStatusHistogramName[] =
    "SubscriptionEligibility.AiSubscriptionTierStatus";
const char kAiSubscriptionTierSyntheticTrialName[] = "AiSubscriptionTier";

std::string GetSyntheticTrialGroupName(AiSubscriptionTierStatus status) {
  switch (status) {
    case AiSubscriptionTierStatus::kValueNotSet:
      NOTREACHED();
    case AiSubscriptionTierStatus::kNoProfilesSubscribed:
      return "NoProfilesSubscribed";
    case AiSubscriptionTierStatus::kSomeProfilesSubscribed:
      return "SomeProfilesSubscribed";
    case AiSubscriptionTierStatus::kAllProfilesSubscribedButDifferentTiers:
      return "AllProfilesSubscribedButDifferentTiers";
    case AiSubscriptionTierStatus::kAllProfilesAtTierEquals1:
      return "Tier1";
    case AiSubscriptionTierStatus::kAllProfilesAtTierEquals2:
      return "Tier2";
    case AiSubscriptionTierStatus::kAllProfilesAtTierEquals3:
      return "Tier3";
    case AiSubscriptionTierStatus::kAllProfilesSubscribedForUnknownTier:
      return "AllProfilesSubscribedForUnknownTier";
  }
}

AiSubscriptionTierStatus ComputeAiSubscriptionTierStatus(
    const std::set<int32_t>& subscription_tiers) {
  if (subscription_tiers.size() > 1) {
    if (subscription_tiers.contains(0)) {
      // Some profiles enabled and some not enabled.
      return AiSubscriptionTierStatus::kSomeProfilesSubscribed;
    }
    // All profiles enabled but at different tiers.
    return AiSubscriptionTierStatus::kAllProfilesSubscribedButDifferentTiers;
  }

  CHECK_EQ(subscription_tiers.size(), 1u);

  if (subscription_tiers.contains(0)) {
    // All profiles not enabled.
    return AiSubscriptionTierStatus::kNoProfilesSubscribed;
  }
  if (subscription_tiers.contains(1)) {
    // All profiles enabled but at tier = 1.
    return AiSubscriptionTierStatus::kAllProfilesAtTierEquals1;
  }
  if (subscription_tiers.contains(2)) {
    // All profiles enabled but at tier = 2.
    return AiSubscriptionTierStatus::kAllProfilesAtTierEquals2;
  }
  if (subscription_tiers.contains(3)) {
    // All profiles enabled but at tier = 3.
    return AiSubscriptionTierStatus::kAllProfilesAtTierEquals3;
  }

  // All profiles enabled but at unknown tier.
  return AiSubscriptionTierStatus::kAllProfilesSubscribedForUnknownTier;
}

}  // namespace subscription_eligibility
