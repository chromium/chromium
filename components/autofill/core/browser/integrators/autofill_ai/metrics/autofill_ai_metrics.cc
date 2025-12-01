// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"

#include <algorithm>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {
namespace {

constexpr std::string_view
    kNumberOfEntitiesConsideredForDeduplicationHistogramName =
        "Autofill.Ai.Deduplication.NumberOfLocalEntitiesConsidered";

constexpr std::string_view kNumberOfEntitiesDedupedHistogramName =
    "Autofill.Ai.Deduplication.NumberOfLocalEntitiesDeduped";

}  // namespace

void LogOptInFunnelEvent(AutofillAiOptInFunnelEvents event) {
  base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Funnel", event);
}

void LogLocalEntitiesDeduplicationMetrics(
    const base::flat_map<EntityType, size_t>&
        local_entities_considered_for_deduplication_per_type,
    const base::flat_map<EntityType, size_t>&
        local_entities_deduplicated_per_type) {
  size_t n_total_entities_considered = 0;
  size_t n_total_entities_removed = 0;

  for (auto const [type, count] :
       local_entities_considered_for_deduplication_per_type) {
    if (count < 2) {
      continue;
    }
    n_total_entities_considered += count;
    base::UmaHistogramCounts100(
        base::StrCat({kNumberOfEntitiesConsideredForDeduplicationHistogramName,
                      ".",
                      {EntityTypeToMetricsString(type)}}),
        count);

    auto it = local_entities_deduplicated_per_type.find(type);
    const size_t n_removed_for_entity =
        (it != local_entities_deduplicated_per_type.end() ? it->second : 0);
    n_total_entities_removed += n_removed_for_entity;

    base::UmaHistogramCounts100(
        base::StrCat({kNumberOfEntitiesDedupedHistogramName, ".",
                      EntityTypeToMetricsString(type)}),
        n_removed_for_entity);
  }

  base::UmaHistogramCounts100(
      base::StrCat({kNumberOfEntitiesConsideredForDeduplicationHistogramName,
                    ".AllEntities"}),
      n_total_entities_considered);
  base::UmaHistogramCounts100(
      base::StrCat({kNumberOfEntitiesDedupedHistogramName, ".AllEntities"}),
      n_total_entities_removed);
}

void LogStoredEntitiesCount(base::span<const EntityInstance> entities) {
  static constexpr std::string_view kHistogramPrefix =
      "Autofill.Ai.StoredEntitiesCount";
  base::flat_map<EntityType, std::vector<const EntityInstance*>>
      entities_by_type;
  for (const EntityInstance& entity : entities) {
    entities_by_type[entity.type()].push_back(&entity);
  }

  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    for (EntityInstance::RecordType record_type :
         DenseSet<EntityInstance::RecordType>::all()) {
      base::UmaHistogramCounts1000(
          base::StrCat({kHistogramPrefix, ".",
                        EntityTypeToMetricsString(entity_type), ".",
                        EntityRecordTypeToMetricsString(record_type)}),
          std::ranges::count(entities_by_type[entity_type], record_type,
                             &EntityInstance::record_type));
    }
    base::UmaHistogramCounts1000(
        base::StrCat(
            {kHistogramPrefix, ".", EntityTypeToMetricsString(entity_type)}),
        entities_by_type[entity_type].size());
  }
}

// LINT.IfChange(EntityTypeToMetricsString)
std::string_view EntityTypeToMetricsString(EntityType type) {
  switch (type.name()) {
    case EntityTypeName::kPassport:
      return "Passport";
    case EntityTypeName::kDriversLicense:
      return "DriversLicense";
    case EntityTypeName::kVehicle:
      return "Vehicle";
    case EntityTypeName::kNationalIdCard:
      return "NationalIdCard";
    case EntityTypeName::kKnownTravelerNumber:
      return "KnownTravelerNumber";
    case EntityTypeName::kRedressNumber:
      return "RedressNumber";
    case EntityTypeName::kFlightReservation:
      return "FlightReservation";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.Ai.EntityType)

// LINT.IfChange(EntityRecordTypeToMetricsString)
std::string_view EntityRecordTypeToMetricsString(
    EntityInstance::RecordType record_type) {
  switch (record_type) {
    case EntityInstance::RecordType::kLocal:
      return "Local";
    case EntityInstance::RecordType::kServerWallet:
      return "ServerWallet";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.Ai.EntityRecordType)

// LINT.IfChange(EntityPromptTypeToMetricsString)
std::string_view EntityPromptTypeToMetricsString(
    AutofillClient::AutofillAiImportPromptType prompt_type) {
  switch (prompt_type) {
    case AutofillClient::AutofillAiImportPromptType::kSave:
      return "SavePrompt";
    case AutofillClient::AutofillAiImportPromptType::kUpdate:
      return "UpdatePrompt";
    case AutofillClient::AutofillAiImportPromptType::kMigrate:
      return "MigratePrompt";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.Ai.EntityPromptType)

}  // namespace autofill
