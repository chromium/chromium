// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/subscription_eligibility_metrics_util.h"

#include <set>

#include "testing/gtest/include/gtest/gtest.h"

namespace subscription_eligibility {

using SubscriptionEligibilityMetricsUtilTest = testing::Test;

TEST_F(SubscriptionEligibilityMetricsUtilTest,
       ComputeAiSubscriptionTierStatus) {
  // Empty should not happen in practice since we exit early, but if it does
  // the CHECKs in ComputeAiSubscriptionTierStatus would trigger. We will not
  // test the crash scenarios for an empty set.

  // All profiles not enabled (only 0).
  EXPECT_EQ(AiSubscriptionTierStatus::kNoProfilesSubscribed,
            ComputeAiSubscriptionTierStatus({0}));

  // Some profiles enabled and some not enabled (e.g. 0 and another tier).
  EXPECT_EQ(AiSubscriptionTierStatus::kSomeProfilesSubscribed,
            ComputeAiSubscriptionTierStatus({0, 1}));
  EXPECT_EQ(AiSubscriptionTierStatus::kSomeProfilesSubscribed,
            ComputeAiSubscriptionTierStatus({0, 2}));
  EXPECT_EQ(AiSubscriptionTierStatus::kSomeProfilesSubscribed,
            ComputeAiSubscriptionTierStatus({0, 1, 2}));

  // All profiles enabled but at different tiers (no 0, but multiple tiers).
  EXPECT_EQ(AiSubscriptionTierStatus::kAllProfilesSubscribedButDifferentTiers,
            ComputeAiSubscriptionTierStatus({1, 2}));
  EXPECT_EQ(AiSubscriptionTierStatus::kAllProfilesSubscribedButDifferentTiers,
            ComputeAiSubscriptionTierStatus({1, 3}));

  // All profiles enabled at tier = 1.
  EXPECT_EQ(AiSubscriptionTierStatus::kAllProfilesAtTierEquals1,
            ComputeAiSubscriptionTierStatus({1}));

  // All profiles enabled at tier = 2.
  EXPECT_EQ(AiSubscriptionTierStatus::kAllProfilesAtTierEquals2,
            ComputeAiSubscriptionTierStatus({2}));

  // All profiles enabled at tier = 3.
  EXPECT_EQ(AiSubscriptionTierStatus::kAllProfilesAtTierEquals3,
            ComputeAiSubscriptionTierStatus({3}));

  // All profiles enabled but at unknown tier.
  EXPECT_EQ(AiSubscriptionTierStatus::kAllProfilesSubscribedForUnknownTier,
            ComputeAiSubscriptionTierStatus({4}));
}

}  // namespace subscription_eligibility
