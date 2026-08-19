// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_ELIGIBILITY_METRICS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_ELIGIBILITY_METRICS_TRACKER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"

class PrefService;

namespace autofill {

// Tracks and records AtMemory Personal Context non-eligibility UMA histograms.
class AtMemoryEligibilityMetricsTracker
    : public personal_context::PersonalContextEligibilityService::Observer,
      public subscription_eligibility::SubscriptionEligibilityService::
          Observer {
 public:
  AtMemoryEligibilityMetricsTracker(
      personal_context::PersonalContextEligibilityService*
          personal_context_eligibility_service,
      subscription_eligibility::SubscriptionEligibilityService*
          subscription_eligibility_service,
      PrefService* pref_service);
  AtMemoryEligibilityMetricsTracker(const AtMemoryEligibilityMetricsTracker&) =
      delete;
  AtMemoryEligibilityMetricsTracker& operator=(
      const AtMemoryEligibilityMetricsTracker&) = delete;
  ~AtMemoryEligibilityMetricsTracker() override;

  // personal_context::PersonalContextEligibilityService::Observer:
  void OnEligibilityStateChanged(
      personal_context::PersonalContextEligibilityState new_state) override;

  // subscription_eligibility::SubscriptionEligibilityService::Observer:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

 private:
  // Invoked when the personal context settings toggle changes.
  void OnPersonalContextSettingsToggleChanged();

  // Computes the non-eligibility reason (e.g. G1 subscription status or Android
  // premium device status) and logs it to UMA if the reason has changed and the
  // startup delay has elapsed.
  void ComputeAndMaybeLogNonEligibilityReason();

  // Indicates whether `kNonEligibilityLoggingDelayOnStartup` has elapsed,
  // preventing premature UMA logging during browser startup.
  bool is_non_eligibility_startup_delay_elapsed_ = false;

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_registrar_;

  // The last reported non-eligibility reason.
  std::optional<personal_context::PersonalContextNonEligibilityReason>
      last_non_eligibility_reason_;

  base::ScopedObservation<
      personal_context::PersonalContextEligibilityService,
      personal_context::PersonalContextEligibilityService::Observer>
      eligibility_service_observation_{this};

  base::ScopedObservation<
      subscription_eligibility::SubscriptionEligibilityService,
      subscription_eligibility::SubscriptionEligibilityService::Observer>
      subscription_eligibility_observation_{this};

  base::WeakPtrFactory<AtMemoryEligibilityMetricsTracker> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_ELIGIBILITY_METRICS_TRACKER_H_
