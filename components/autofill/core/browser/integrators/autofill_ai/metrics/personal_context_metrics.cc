// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager.h"

namespace autofill {

void LogPersonalContextCacheReadinessOnFirstInteraction(
    EntityType type,
    PersonalContextCacheReadinessOnFirstInteraction readiness) {
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PersonalContext.Cache.ReadinessOnFirstInteraction",
      readiness);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Autofill.Ai.PersonalContext.Cache.ReadinessOnFirstInteraction.",
           EntityTypeToMetricsString(type)}),
      readiness);
}

PersonalContextCacheReadinessOnFirstInteraction GetCacheReadinessState(
    const AutofillAiPersonalContextAccessManager& access_manager,
    const EntityDataManager* entity_data_manager,
    EntityType entity_type) {
  using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;
  RequestStatus status =
      access_manager.GetPrefetchStatusByEntityType(entity_type);
  switch (status) {
    case RequestStatus::kNotStarted:
      // Prefetch was not initiated (ineligible or type unsupported).
      return PersonalContextCacheReadinessOnFirstInteraction::kNotStarted;
    case RequestStatus::kPending:
      // Prefetch request is in-flight when the user interacts.
      return PersonalContextCacheReadinessOnFirstInteraction::kPendingInFlight;
    case RequestStatus::kFailure:
      // Prefetch failed.
      return PersonalContextCacheReadinessOnFirstInteraction::kFailed;
    case RequestStatus::kSuccess: {
      // Prefetch succeeded. We check if the cache contains either sensitive
      // (SPII) or non-sensitive entity data.
      const bool has_spii_signal =
          access_manager.ServerHasSpiiPresenceSignal(entity_type);
      const bool has_entity_data =
          entity_data_manager &&
          std::ranges::any_of(entity_data_manager->GetEntityInstances(),
                              [&](const EntityInstance& entity) {
                                return entity.type() == entity_type;
                              });
      return (has_spii_signal || has_entity_data)
                 ? PersonalContextCacheReadinessOnFirstInteraction::
                       kResolvedWithData
                 : PersonalContextCacheReadinessOnFirstInteraction::
                       kResolvedEmpty;
    }
  }

  NOTREACHED();
}

void LogPersonalContextPrefetchTriggerResults(
    const DenseSet<PersonalContextPrefetchTriggerResult>&
        unique_trigger_results) {
  for (PersonalContextPrefetchTriggerResult trigger_result :
       unique_trigger_results) {
    base::UmaHistogramEnumeration(
        "Autofill.Ai.PersonalContext.Prefetch.TriggerResult", trigger_result);
  }
}

void LogPersonalContextPrefetchTotalLatency(EntityType type,
                                            base::TimeDelta latency) {
  base::UmaHistogramMediumTimes(
      base::StrCat({"Autofill.Ai.PersonalContext.Prefetch.TotalLatency.",
                    EntityTypeToMetricsString(type)}),
      latency);
}

void LogPersonalContextNonEligibilityReason(
    personal_context::PersonalContextNonEligibilityReason reason) {
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PersonalContext.NonEligibilityReason", reason);
}

void LogPersonalContextPrefetchEntityValidationResult(
    EntityType type,
    PersonalContextPrefetchEntityValidationResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PersonalContext.Prefetch.EntityValidationResult", result);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Autofill.Ai.PersonalContext.Prefetch.EntityValidationResult.",
           EntityTypeToMetricsString(type)}),
      result);
}

void LogPersonalContextPrefetchEntitySourceCount(EntityType type,
                                                 size_t count) {
  base::UmaHistogramCounts100(
      "Autofill.Ai.PersonalContext.Prefetch.EntitySourceCount", count);
  base::UmaHistogramCounts100(
      base::StrCat({"Autofill.Ai.PersonalContext.Prefetch.EntitySourceCount.",
                    EntityTypeToMetricsString(type)}),
      count);
}

void LogPersonalContextDeduplicationAgainstRecordType(
    EntityType type,
    std::optional<EntityInstance::RecordType> record_type) {
  PersonalContextDeduplicationAgainstRecordType sample =
      PersonalContextDeduplicationAgainstRecordType::kNotDeduplicated;
  if (record_type) {
    switch (*record_type) {
      case EntityInstance::RecordType::kLocal:
        sample = PersonalContextDeduplicationAgainstRecordType::kLocal;
        break;
      case EntityInstance::RecordType::kServerWallet:
        sample = PersonalContextDeduplicationAgainstRecordType::kServerWallet;
        break;
      case EntityInstance::RecordType::kPersonalContext:
        // Personal Context entities are not deduplicated against each other.
        NOTREACHED();
    }
  }
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PersonalContext.Deduplication.AgainstRecordType", sample);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Autofill.Ai.PersonalContext.Deduplication.AgainstRecordType.",
           EntityTypeToMetricsString(type)}),
      sample);
}

void LogPersonalContextPrefetchEntityFunnelOutcome(
    std::optional<EntityType> type,
    PersonalContextPrefetchEntityFunnelOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PersonalContext.Prefetch.EntityFunnel", outcome);
  if (type) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Autofill.Ai.PersonalContext.Prefetch.EntityFunnel.",
                      EntityTypeToMetricsString(*type)}),
        outcome);
  }
}

void LogEntitySuppressionEntitiesFilteredPerPrefetch(size_t count) {
  base::UmaHistogramCounts100(
      "Autofill.Ai.Suppression.EntitiesFilteredPerPrefetch", count);
}

void LogEntitySuppressedWithSavedEntityPresent(
    EntityType type,
    const EntityDataManager* entity_data_manager) {
  const bool has_saved_entity =
      entity_data_manager &&
      std::ranges::any_of(
          entity_data_manager->GetEntityInstances(),
          [&](const EntityInstance& entity) {
            return entity.type() == type &&
                   (entity.record_type() ==
                        EntityInstance::RecordType::kLocal ||
                    entity.record_type() ==
                        EntityInstance::RecordType::kServerWallet);
          });
  base::UmaHistogramBoolean(
      "Autofill.Ai.Suppression.SavedEntityOfSameTypePresent", has_saved_entity);
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Ai.Suppression.SavedEntityOfSameTypePresent.",
                    EntityTypeToMetricsString(type)}),
      has_saved_entity);
}

void LogEntitySuppressionLoadDroppedRecordCount(size_t count) {
  base::UmaHistogramCounts1000(
      "Autofill.Ai.Suppression.Load.DroppedRecordCount", count);
}

void LogEntitySuppressionLoadDropReason(
    EntitySuppressionLoadDropReason reason) {
  base::UmaHistogramEnumeration("Autofill.Ai.Suppression.Load.DropReason",
                                reason);
}

void LogEntitySuppressionConfirmationDialogResult(bool accepted) {
  base::UmaHistogramEnumeration(
      "Autofill.Ai.Suppression.ConfirmationDialogResult",
      accepted ? EntitySuppressionConfirmationDialogResult::kAccepted
               : EntitySuppressionConfirmationDialogResult::kCancelled);
}

}  // namespace autofill
