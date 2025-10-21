// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
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
  // TODO(crbug.com/408380915): Remove after M141.
  base::UmaHistogramEnumeration("Autofill.Ai.OptInFunnel", event);
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

void LogLocalEntitiesDeduplicationMetrics(
    const base::flat_map<EntityType, size_t>&
        local_entities_considered_for_deduplication_per_type,
    const base::flat_map<EntityType, size_t>&
        local_entities_dedupled_per_type) {
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

    auto it = local_entities_dedupled_per_type.find(type);
    const size_t n_removed_for_entity =
        (it != local_entities_dedupled_per_type.end() ? it->second : 0);
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

}  // namespace autofill
