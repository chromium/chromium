// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_eligibility_metrics_tracker.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"

namespace autofill {

AtMemoryEligibilityMetricsTracker::AtMemoryEligibilityMetricsTracker(
    personal_context::PersonalContextEligibilityService*
        personal_context_eligibility_service,
    subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
    PrefService* pref_service)
    : pref_service_(pref_service) {
  if (personal_context_eligibility_service) {
    eligibility_service_observation_.Observe(
        personal_context_eligibility_service);
  }
  if (subscription_eligibility_service) {
    subscription_eligibility_observation_.Observe(
        subscription_eligibility_service);
  }
  if (pref_service_) {
    pref_registrar_.Init(pref_service_);
    pref_registrar_.Add(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        base::BindRepeating(&AtMemoryEligibilityMetricsTracker::
                                OnPersonalContextSettingsToggleChanged,
                            base::Unretained(this)));
  }
  // Called after the startup delay (`kNonEligibilityLoggingDelayOnStartup`)
  // has elapsed to enable non-eligibility UMA logging and record the initial
  // reason.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryEligibilityMetricsTracker> self) {
            if (!self) {
              return;
            }
            self->is_non_eligibility_startup_delay_elapsed_ = true;
            self->ComputeAndMaybeLogNonEligibilityReason();
          },
          weak_ptr_factory_.GetWeakPtr()),
      kNonEligibilityLoggingDelayOnStartup);
}

AtMemoryEligibilityMetricsTracker::~AtMemoryEligibilityMetricsTracker() =
    default;

void AtMemoryEligibilityMetricsTracker::OnEligibilityStateChanged(
    personal_context::PersonalContextEligibilityState /*new_state*/) {
  ComputeAndMaybeLogNonEligibilityReason();
}

void AtMemoryEligibilityMetricsTracker::OnAiSubscriptionTierUpdated(
    int32_t /*new_subscription_tier*/) {
  ComputeAndMaybeLogNonEligibilityReason();
}

void AtMemoryEligibilityMetricsTracker::
    OnPersonalContextSettingsToggleChanged() {
  ComputeAndMaybeLogNonEligibilityReason();
}

void AtMemoryEligibilityMetricsTracker::
    ComputeAndMaybeLogNonEligibilityReason() {
  using personal_context::PersonalContextNonEligibilityReason;
  if (!pref_service_ || !eligibility_service_observation_.IsObserving() ||
      !is_non_eligibility_startup_delay_elapsed_) {
    return;
  }

  std::optional<PersonalContextNonEligibilityReason> non_eligibility_reason =
      eligibility_service_observation_.GetSource()->GetNonEligibilityReason();

  if (non_eligibility_reason ==
          PersonalContextNonEligibilityReason::kEligible &&
      !IsDeviceOrSubscriptionTierEligibleForAtMemory(
          subscription_eligibility_observation_.GetSource())) {
    non_eligibility_reason = PersonalContextNonEligibilityReason::
        kNotG1SubscriberOrAndroidPremiumDevice;
  }

  if (non_eligibility_reason ==
          PersonalContextNonEligibilityReason::kEligible &&
      !pref_service_->GetBoolean(
          personal_context::prefs::
              kPersonalContextInAutofillSettingsToggleStatus)) {
    non_eligibility_reason =
        PersonalContextNonEligibilityReason::kPersonalIntelligencePrefDisabled;
  }

  if (last_non_eligibility_reason_ == non_eligibility_reason) {
    return;
  }
  last_non_eligibility_reason_ = non_eligibility_reason;
  if (last_non_eligibility_reason_) {
    base::UmaHistogramEnumeration(
        "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
        *last_non_eligibility_reason_);
  }
}

}  // namespace autofill
