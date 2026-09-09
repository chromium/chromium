// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_PERSONAL_CONTEXT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_PERSONAL_CONTEXT_METRICS_H_

#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/personal_context/core/personal_context_types.h"

namespace autofill {

// Delay before logging the non-eligibility reason on startup. Instead of
// reporting immediately at startup (which would incorrectly report non-eligible
// before preferences are loaded from disk), this delay ensures initial
// preference and device state have been populated.
inline constexpr base::TimeDelta kNonEligibilityLoggingDelayOnStartup =
    base::Seconds(30);

class AutofillAiPersonalContextAccessManager;
class EntityDataManager;

// LINT.IfChange(AutofillAiPersonalContextCacheReadinessOnFirstInteraction)
enum class PersonalContextCacheReadinessOnFirstInteraction {
  kResolvedWithData = 0,
  kResolvedEmpty = 1,
  kPendingInFlight = 2,
  kFailed = 3,
  kNotStarted = 4,
  kMaxValue = kNotStarted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiPersonalContextCacheReadinessOnFirstInteraction)

// LINT.IfChange(AutofillAiPersonalContextPrefetchTriggerResult)
// Represents the outcome when a prefetch trigger is evaluated for a requested
// entity type. Logged to UMA.
enum class PersonalContextPrefetchTriggerResult {
  // A new network request to the backend service is initiated (Cache Miss).
  kInitiated = 0,
  // The fetch is skipped because the cached data is still fresh.
  kSkippedFreshCache = 1,
  // The fetch is skipped because a recent fetch failed and the retry delay
  // configured in exponential backoff has not yet expired.
  kSkippedBackoff = 2,
  // The fetch is skipped because a request for the type is already in-flight.
  kSkippedInFlight = 3,
  kMaxValue = kSkippedInFlight,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiPersonalContextPrefetchTriggerResult)

// LINT.IfChange(AutofillAiPersonalContextPrefetchEntityValidationResult)
// Represents the result of validating an entity received from Personal Context
// during Ambient Autofill prefetch. Logged to UMA.
enum class PersonalContextPrefetchEntityValidationResult {
  // Entity satisfies all import constraints and freshness TTL requirements.
  kValid = 0,
  // Entity does not meet schema import constraints (missing required
  // attributes).
  kFailedImportConstraints = 1,
  // Entity has expired beyond the allowable TTL freshness window.
  kFailedTtlExpired = 2,
  // Entity is missing the required date attribute used for TTL evaluation.
  kFailedTtlMissingDate = 3,
  // Entity date attribute cannot be parsed into a valid YYYY-MM-DD date.
  kFailedTtlInvalidDate = 4,
  // Entity type is not supported by Ambient Autofill (e.g. KTN or Redress
  // Number).
  kUnsupportedEntityType = 5,
  kMaxValue = kUnsupportedEntityType,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiPersonalContextPrefetchEntityValidationResult)

// Returns the readiness state of the prefetch cache for `entity_type` when the
// user first interacts with an Ambient Autofill supported field of that type.
PersonalContextCacheReadinessOnFirstInteraction GetCacheReadinessState(
    const AutofillAiPersonalContextAccessManager& access_manager,
    const EntityDataManager* entity_data_manager,
    EntityType entity_type);

// Logs the readiness state of the prefetch cache on the user's first
// interaction with an Ambient Autofill supported field.
void LogPersonalContextCacheReadinessOnFirstInteraction(
    EntityType type,
    PersonalContextCacheReadinessOnFirstInteraction readiness);

// Logs the unique prefetch trigger outcomes present in a batch of requested
// entity types to UMA. Each outcome type is logged at most once per prefetch
// request.
void LogPersonalContextPrefetchTriggerResults(
    const DenseSet<PersonalContextPrefetchTriggerResult>&
        unique_trigger_results);

// Logs the total latency for a prefetch request of a specific `type`.
void LogPersonalContextPrefetchTotalLatency(EntityType type,
                                            base::TimeDelta latency);

// Logs the non-eligibility reason for personal context.
void LogPersonalContextNonEligibilityReason(
    personal_context::PersonalContextNonEligibilityReason reason);

// Logs the validation outcome when filtering entities extracted from Personal
// Context prefetch responses for Ambient Autofill.
void LogPersonalContextPrefetchEntityValidationResult(
    EntityType type,
    PersonalContextPrefetchEntityValidationResult result);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_PERSONAL_CONTEXT_METRICS_H_
