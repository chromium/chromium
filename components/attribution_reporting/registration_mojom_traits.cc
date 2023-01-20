// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_mojom_traits.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom-shared.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/registration.mojom-shared.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_attestation.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "mojo/public/cpp/base/int128_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

// static
bool StructTraits<attribution_reporting::mojom::SuitableOriginDataView,
                  attribution_reporting::SuitableOrigin>::
    Read(attribution_reporting::mojom::SuitableOriginDataView data,
         attribution_reporting::SuitableOrigin* out) {
  url::Origin origin;
  if (!data.ReadOrigin(&origin)) {
    return false;
  }

  auto suitable_origin =
      attribution_reporting::SuitableOrigin::Create(std::move(origin));
  if (!suitable_origin) {
    return false;
  }

  *out = std::move(*suitable_origin);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::FilterDataDataView,
                  attribution_reporting::FilterData>::
    Read(attribution_reporting::mojom::FilterDataDataView data,
         attribution_reporting::FilterData* out) {
  attribution_reporting::FilterValues filter_values;
  if (!data.ReadFilterValues(&filter_values)) {
    return false;
  }

  absl::optional<attribution_reporting::FilterData> filter_data =
      attribution_reporting::FilterData::Create(std::move(filter_values));
  if (!filter_data.has_value()) {
    return false;
  }

  *out = std::move(*filter_data);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::AggregationKeysDataView,
                  attribution_reporting::AggregationKeys>::
    Read(attribution_reporting::mojom::AggregationKeysDataView data,
         attribution_reporting::AggregationKeys* out) {
  attribution_reporting::AggregationKeys::Keys keys;
  if (!data.ReadKeys(&keys)) {
    return false;
  }

  absl::optional<attribution_reporting::AggregationKeys> aggregation_keys =
      attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
  if (!aggregation_keys.has_value()) {
    return false;
  }

  *out = std::move(*aggregation_keys);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::SourceRegistrationDataView,
                  attribution_reporting::SourceRegistration>::
    Read(attribution_reporting::mojom::SourceRegistrationDataView data,
         attribution_reporting::SourceRegistration* out) {
  if (!data.ReadDestination(&out->destination)) {
    return false;
  }

  if (!data.ReadExpiry(&out->expiry)) {
    return false;
  }

  if (!data.ReadEventReportWindow(&out->event_report_window)) {
    return false;
  }

  if (!data.ReadAggregatableReportWindow(&out->aggregatable_report_window)) {
    return false;
  }

  if (!data.ReadDebugKey(&out->debug_key)) {
    return false;
  }

  if (!data.ReadFilterData(&out->filter_data)) {
    return false;
  }

  if (!data.ReadAggregationKeys(&out->aggregation_keys)) {
    return false;
  }

  out->source_event_id = data.source_event_id();
  out->priority = data.priority();
  out->debug_reporting = data.debug_reporting();
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::FiltersDataView,
                  attribution_reporting::Filters>::
    Read(attribution_reporting::mojom::FiltersDataView data,
         attribution_reporting::Filters* out) {
  attribution_reporting::FilterValues filter_values;
  if (!data.ReadFilterValues(&filter_values)) {
    return false;
  }

  absl::optional<attribution_reporting::Filters> filters =
      attribution_reporting::Filters::Create(std::move(filter_values));
  if (!filters.has_value()) {
    return false;
  }

  *out = std::move(*filters);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::TriggerAttestationDataView,
                  attribution_reporting::TriggerAttestation>::
    Read(attribution_reporting::mojom::TriggerAttestationDataView data,
         attribution_reporting::TriggerAttestation* out) {
  std::string token;
  if (!data.ReadToken(&token)) {
    return false;
  }

  std::string aggregatable_report_id;
  if (!data.ReadAggregatableReportId(&aggregatable_report_id)) {
    return false;
  }

  auto trigger_attesation = attribution_reporting::TriggerAttestation::Create(
      std::move(token), aggregatable_report_id);
  if (!trigger_attesation) {
    return false;
  }

  *out = std::move(*trigger_attesation);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::EventTriggerDataDataView,
                  attribution_reporting::EventTriggerData>::
    Read(attribution_reporting::mojom::EventTriggerDataDataView data,
         attribution_reporting::EventTriggerData* out) {
  if (!data.ReadDedupKey(&out->dedup_key)) {
    return false;
  }

  if (!data.ReadFilters(&out->filters)) {
    return false;
  }

  if (!data.ReadNotFilters(&out->not_filters)) {
    return false;
  }

  out->data = data.data();
  out->priority = data.priority();
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::AggregatableTriggerDataDataView,
                  attribution_reporting::AggregatableTriggerData>::
    Read(attribution_reporting::mojom::AggregatableTriggerDataDataView data,
         attribution_reporting::AggregatableTriggerData* out) {
  absl::uint128 key_piece;
  if (!data.ReadKeyPiece(&key_piece)) {
    return false;
  }

  attribution_reporting::AggregatableTriggerData::Keys source_keys;
  if (!data.ReadSourceKeys(&source_keys)) {
    return false;
  }

  attribution_reporting::Filters filters;
  if (!data.ReadFilters(&filters)) {
    return false;
  }

  attribution_reporting::Filters not_filters;
  if (!data.ReadNotFilters(&not_filters)) {
    return false;
  }

  auto aggregatable_trigger_data =
      attribution_reporting::AggregatableTriggerData::Create(
          key_piece, std::move(source_keys), std::move(filters),
          std::move(not_filters));
  if (!aggregatable_trigger_data) {
    return false;
  }

  *out = std::move(*aggregatable_trigger_data);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::TriggerRegistrationDataView,
                  attribution_reporting::TriggerRegistration>::
    Read(attribution_reporting::mojom::TriggerRegistrationDataView data,
         attribution_reporting::TriggerRegistration* out) {
  std::vector<attribution_reporting::EventTriggerData> event_triggers;
  if (!data.ReadEventTriggers(&event_triggers)) {
    return false;
  }

  auto event_triggers_list =
      attribution_reporting::EventTriggerDataList::Create(
          std::move(event_triggers));
  if (!event_triggers_list) {
    return false;
  }

  out->event_triggers = std::move(*event_triggers_list);

  if (!data.ReadFilters(&out->filters)) {
    return false;
  }

  if (!data.ReadNotFilters(&out->not_filters)) {
    return false;
  }

  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data;
  if (!data.ReadAggregatableTriggerData(&aggregatable_trigger_data)) {
    return false;
  }

  auto aggregatable_trigger_data_list =
      attribution_reporting::AggregatableTriggerDataList::Create(
          std::move(aggregatable_trigger_data));
  if (!aggregatable_trigger_data_list) {
    return false;
  }

  out->aggregatable_trigger_data = std::move(*aggregatable_trigger_data_list);

  attribution_reporting::AggregatableValues::Values values;
  if (!data.ReadAggregatableValues(&values)) {
    return false;
  }

  auto aggregatable_values =
      attribution_reporting::AggregatableValues::Create(std::move(values));
  if (!aggregatable_values) {
    return false;
  }

  out->aggregatable_values = std::move(*aggregatable_values);

  if (!data.ReadDebugKey(&out->debug_key)) {
    return false;
  }

  if (!data.ReadAggregatableDedupKey(&out->aggregatable_dedup_key)) {
    return false;
  }

  out->debug_reporting = data.debug_reporting();
  out->aggregation_coordinator = data.aggregation_coordinator();
  return true;
}

}  // namespace mojo
