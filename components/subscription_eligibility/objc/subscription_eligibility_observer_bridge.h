// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBSCRIPTION_ELIGIBILITY_OBJC_SUBSCRIPTION_ELIGIBILITY_OBSERVER_BRIDGE_H_
#define COMPONENTS_SUBSCRIPTION_ELIGIBILITY_OBJC_SUBSCRIPTION_ELIGIBILITY_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"

// Implement this protocol and pass your implementation into a
// SubscriptionEligibilityObserverBridge object to receive
// SubscriptionEligibilityService observer callbacks in Objective-C.
@protocol SubscriptionEligibilityServiceObserving <NSObject>

@optional

// Invoked when the AI Subscription tier has been updated.
- (void)aiSubscriptionTierDidUpdate:(int32_t)newSubscriptionTier;

@end

namespace subscription_eligibility {

// Bridge class that listens for |SubscriptionEligibilityService| notifications
// and passes them to its Objective-C observer.
class SubscriptionEligibilityObserverBridge
    : public SubscriptionEligibilityService::Observer {
 public:
  SubscriptionEligibilityObserverBridge(
      SubscriptionEligibilityService* subscription_eligibility_service,
      id<SubscriptionEligibilityServiceObserving> observer);

  SubscriptionEligibilityObserverBridge(
      const SubscriptionEligibilityObserverBridge&) = delete;
  SubscriptionEligibilityObserverBridge& operator=(
      const SubscriptionEligibilityObserverBridge&) = delete;

  ~SubscriptionEligibilityObserverBridge() override;

  // SubscriptionEligibilityService::Observer.
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

 private:
  base::ScopedObservation<SubscriptionEligibilityService,
                          SubscriptionEligibilityService::Observer>
      subscription_eligibility_service_observation_{this};
  // Observer to call.
  __weak id<SubscriptionEligibilityServiceObserving> observer_;
};

}  // namespace subscription_eligibility

#endif  // COMPONENTS_SUBSCRIPTION_ELIGIBILITY_OBJC_SUBSCRIPTION_ELIGIBILITY_OBSERVER_BRIDGE_H_
