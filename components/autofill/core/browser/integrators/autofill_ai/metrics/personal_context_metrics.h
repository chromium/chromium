// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_PERSONAL_CONTEXT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_PERSONAL_CONTEXT_METRICS_H_

#include <cstddef>
#include <optional>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
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
  // Entity does not have any source attached in its payload.
  kFailedMissingSource = 6,
  kMaxValue = kFailedMissingSource,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiPersonalContextPrefetchEntityValidationResult)

// LINT.IfChange(AutofillAiPersonalContextDeduplicationAgainstRecordType)
// The record type of the entity that a Personal Context entity is
// deduplicated against. Logged to UMA.
enum class PersonalContextDeduplicationAgainstRecordType {
  kLocal = 0,
  kServerWallet = 1,
  // The Personal Context entity is not a duplicate of any entity of a
  // different record type.
  kNotDeduplicated = 2,
  kMaxValue = kNotDeduplicated,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiPersonalContextDeduplicationAgainstRecordType)

// LINT.IfChange(AutofillAiPersonalContextPrefetchEntityFunnelOutcome)
// Represents the stage at which an entity received from Personal Context during
// Ambient Autofill prefetch is dropped, or `kPassed` if it passes all stages.
// Logged to UMA.
enum class PersonalContextPrefetchEntityFunnelOutcome {
  // The entity cannot be converted into an `EntityInstance`, e.g. because it
  // cannot be decrypted or its entity type is unknown.
  kParsingFailed = 0,
  // The entity does not satisfy the requirements of Ambient Autofill, e.g.
  // because it is missing required attributes, has expired or is of a type that
  // was not requested.
  kInvalid = 1,
  // The entity is suppressed by the user.
  kSuppressed = 2,
  // The entity is a duplicate of a local or Wallet entity.
  kDeduplicated = 3,
  // The entity passes all stages.
  kPassed = 4,
  kMaxValue = kPassed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiPersonalContextPrefetchEntityFunnelOutcome)

// LINT.IfChange(AutofillAiEntitySuppressionLoadDropReason)
// Reasons why a stored entity suppression record is dropped when loading
// suppressions from disk. Logged to UMA.
enum class EntitySuppressionLoadDropReason {
  // The record could not be decrypted, e.g. because the OS encryption key
  // changed.
  kDecryptionFailed = 0,
  // The decrypted record could not be parsed.
  kParseFailed = 1,
  // The parsed record does not describe a valid suppression entry.
  kInvalidEntry = 2,
  kMaxValue = kInvalidEntry,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiEntitySuppressionLoadDropReason)

// LINT.IfChange(AutofillAiEntitySuppressionConfirmationDialogResult)
// How the user closed the dialog asking to confirm the suppression of an
// entity. Logged to UMA.
enum class EntitySuppressionConfirmationDialogResult {
  // The user confirmed the suppression.
  kAccepted = 0,
  // The user cancelled or dismissed the dialog.
  kCancelled = 1,
  kMaxValue = kCancelled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiEntitySuppressionConfirmationDialogResult)

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

// Logs the number of valid sources attached to an entity extracted from
// Personal Context prefetch responses for Ambient Autofill.
void LogPersonalContextPrefetchEntitySourceCount(EntityType type, size_t count);

// Logs the record type of the entity that a Personal Context entity of `type`
// is deduplicated against, or that it is not deduplicated if `record_type` is
// `std::nullopt`.
void LogPersonalContextDeduplicationAgainstRecordType(
    EntityType type,
    std::optional<EntityInstance::RecordType> record_type);

// Logs the `outcome` of an entity received from Personal Context during Ambient
// Autofill prefetch. The per-type histogram is only logged if the `type` is
// known, which is not the case if the entity cannot be parsed.
void LogPersonalContextPrefetchEntityFunnelOutcome(
    std::optional<EntityType> type,
    PersonalContextPrefetchEntityFunnelOutcome outcome);

// Logs the number of entities filtered out due to suppression during prefetch.
void LogEntitySuppressionEntitiesFilteredPerPrefetch(size_t count);

// Logs whether the user already has at least one saved (`kLocal` or
// `kServerWallet`) entity of `type` when manually suppressing a Personal
// Context entity.
void LogEntitySuppressedWithSavedEntityPresent(
    EntityType type,
    const EntityDataManager* entity_data_manager);

// Logs the number of stored suppression records that were dropped while
// loading suppressions from disk.
void LogEntitySuppressionLoadDroppedRecordCount(size_t count);

// Logs why a stored suppression record was dropped while loading suppressions
// from disk.
void LogEntitySuppressionLoadDropReason(EntitySuppressionLoadDropReason reason);

// Logs whether the user `accepted` the suppression confirmation dialog.
void LogEntitySuppressionConfirmationDialogResult(bool accepted);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_PERSONAL_CONTEXT_METRICS_H_
