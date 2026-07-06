// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/subscription_eligibility/objc/subscription_eligibility_observer_bridge.h"

namespace subscription_eligibility {

SubscriptionEligibilityObserverBridge::SubscriptionEligibilityObserverBridge(
    SubscriptionEligibilityService* subscription_eligibility_service,
    id<SubscriptionEligibilityServiceObserving> observer)
    : observer_(observer) {
  subscription_eligibility_service_observation_.Observe(
      subscription_eligibility_service);
}

SubscriptionEligibilityObserverBridge::
    ~SubscriptionEligibilityObserverBridge() = default;

void SubscriptionEligibilityObserverBridge::OnAiSubscriptionTierUpdated(
    int32_t new_subscription_tier) {
  if ([observer_ respondsToSelector:@selector(aiSubscriptionTierDidUpdate:)]) {
    [observer_ aiSubscriptionTierDidUpdate:new_subscription_tier];
  }
}

}  // namespace subscription_eligibility
