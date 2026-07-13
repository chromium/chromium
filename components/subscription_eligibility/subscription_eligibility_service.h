// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_H_
#define COMPONENTS_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace subscription_eligibility {

// Command-line switch to force the AI subscription tier to a specific value.
extern const char kForceAiSubscriptionTier[];

class SubscriptionEligibilityService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the AI Subscription tier has been updated.
    virtual void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) {}
  };

  explicit SubscriptionEligibilityService(PrefService* pref_service);
  SubscriptionEligibilityService(const SubscriptionEligibilityService&) =
      delete;
  SubscriptionEligibilityService operator=(
      const SubscriptionEligibilityService&) = delete;
  ~SubscriptionEligibilityService() override;

  // Returns the AI subscription tier for the user. If a positive tier is
  // returned, it means the account must have Chrome benefits.
  int32_t GetAiSubscriptionTier() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Invoked when underlying pref for ai subscription tier changes.
  void OnAiSubscriptionTierUpdated();

  // Not owned. Guaranteed to outlive `this`.
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_registrar_;

  base::ObserverList<Observer> observers_;

  std::optional<int32_t> forced_tier_;
};

}  // namespace subscription_eligibility

#endif  // COMPONENTS_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_H_
