// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_mojom_traits.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration.mojom-shared.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "mojo/public/cpp/base/int128_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
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

  *out = *std::move(suitable_origin);
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

  std::optional<attribution_reporting::FilterData> filter_data =
      attribution_reporting::FilterData::Create(std::move(filter_values));
  if (!filter_data.has_value()) {
    return false;
  }

  *out = *std::move(filter_data);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::FilterConfigDataView,
                  attribution_reporting::FilterConfig>::
    Read(attribution_reporting::mojom::FilterConfigDataView data,
         attribution_reporting::FilterConfig* out) {
  attribution_reporting::FilterValues filter_values;
  if (!data.ReadFilterValues(&filter_values)) {
    return false;
  }

  std::optional<base::TimeDelta> lookback_window;
  if (!data.ReadLookbackWindow(&lookback_window)) {
    return false;
  }

  auto config = attribution_reporting::FilterConfig::Create(
      std::move(filter_values), lookback_window);
  if (!config.has_value()) {
    return false;
  }
  *out = std::move(config.value());

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

  std::optional<attribution_reporting::AggregationKeys> aggregation_keys =
      attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
  if (!aggregation_keys.has_value()) {
    return false;
  }

  *out = *std::move(aggregation_keys);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::DestinationSetDataView,
                  attribution_reporting::DestinationSet>::
    Read(attribution_reporting::mojom::DestinationSetDataView data,
         attribution_reporting::DestinationSet* out) {
  std::vector<net::SchemefulSite> destinations;
  if (!data.ReadDestinations(&destinations)) {
    return false;
  }

  auto destination_set =
      attribution_reporting::DestinationSet::Create(std::move(destinations));
  if (!destination_set.has_value()) {
    return false;
  }

  *out = *std::move(destination_set);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::EventReportWindowsDataView,
                  attribution_reporting::EventReportWindows>::
    Read(attribution_reporting::mojom::EventReportWindowsDataView data,
         attribution_reporting::EventReportWindows* out) {
  base::TimeDelta start_time;
  if (!data.ReadStartTime(&start_time)) {
    return false;
  }

  std::vector<base::TimeDelta> end_times;
  if (!data.ReadEndTimes(&end_times)) {
    return false;
  }

  auto event_report_windows = attribution_reporting::EventReportWindows::Create(
      start_time, std::move(end_times));
  if (!event_report_windows.has_value()) {
    return false;
  }

  *out = *std::move(event_report_windows);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::TriggerSpecDataView,
                  attribution_reporting::TriggerSpec>::
    Read(attribution_reporting::mojom::TriggerSpecDataView data,
         attribution_reporting::TriggerSpec* out) {
  attribution_reporting::EventReportWindows event_report_windows;
  if (!data.ReadEventReportWindows(&event_report_windows)) {
    return false;
  }

  *out = attribution_reporting::TriggerSpec(std::move(event_report_windows));
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::TriggerSpecsDataView,
                  attribution_reporting::TriggerSpecs>::
    Read(attribution_reporting::mojom::TriggerSpecsDataView data,
         attribution_reporting::TriggerSpecs* out) {
  std::vector<attribution_reporting::TriggerSpec> specs;
  if (!data.ReadSpecs(&specs)) {
    return false;
  }

  attribution_reporting::TriggerSpecs::TriggerDataIndices trigger_data_indices;
  if (!data.ReadTriggerDataIndices(&trigger_data_indices)) {
    return false;
  }

  attribution_reporting::MaxEventLevelReports max_event_level_reports;
  if (!max_event_level_reports.SetIfValid(data.max_event_level_reports())) {
    return false;
  }

  auto result = attribution_reporting::TriggerSpecs::Create(
      std::move(trigger_data_indices), std::move(specs),
      max_event_level_reports);
  if (!result.has_value()) {
    return false;
  }

  *out = *std::move(result);
  return true;
}

// static
bool StructTraits<
    attribution_reporting::mojom::
        AggregatableDebugReportingContributionDataView,
    attribution_reporting::AggregatableDebugReportingContribution>::
    Read(attribution_reporting::mojom::
             AggregatableDebugReportingContributionDataView data,
         attribution_reporting::AggregatableDebugReportingContribution* out) {
  absl::uint128 key_piece;
  if (!data.ReadKeyPiece(&key_piece)) {
    return false;
  }

  std::optional<attribution_reporting::AggregatableDebugReportingContribution>
      contribution =
          attribution_reporting::AggregatableDebugReportingContribution::Create(
              key_piece, data.value());
  if (!contribution.has_value()) {
    return false;
  }

  *out = *std::move(contribution);
  return true;
}

// static
bool StructTraits<
    attribution_reporting::mojom::AggregatableDebugReportingConfigDataView,
    attribution_reporting::AggregatableDebugReportingConfig>::
    Read(attribution_reporting::mojom::AggregatableDebugReportingConfigDataView
             data,
         attribution_reporting::AggregatableDebugReportingConfig* out) {
  absl::uint128 key_piece;
  if (!data.ReadKeyPiece(&key_piece)) {
    return false;
  }

  attribution_reporting::AggregatableDebugReportingConfig::DebugData debug_data;
  if (!data.ReadDebugData(&debug_data)) {
    return false;
  }

  std::optional<attribution_reporting::SuitableOrigin>
      aggregation_coordinator_origin;
  if (!data.ReadAggregationCoordinatorOrigin(&aggregation_coordinator_origin)) {
    return false;
  }

  *out = attribution_reporting::AggregatableDebugReportingConfig(
      key_piece, std::move(debug_data),
      std::move(aggregation_coordinator_origin));
  return true;
}

// static
bool StructTraits<
    attribution_reporting::mojom::
        SourceAggregatableDebugReportingConfigDataView,
    attribution_reporting::SourceAggregatableDebugReportingConfig>::
    Read(attribution_reporting::mojom::
             SourceAggregatableDebugReportingConfigDataView data,
         attribution_reporting::SourceAggregatableDebugReportingConfig* out) {
  attribution_reporting::AggregatableDebugReportingConfig config;
  if (!data.ReadConfig(&config)) {
    return false;
  }

  std::optional<attribution_reporting::SourceAggregatableDebugReportingConfig>
      source_config =
          attribution_reporting::SourceAggregatableDebugReportingConfig::Create(
              data.budget(), std::move(config));
  if (!source_config.has_value()) {
    return false;
  }

  *out = *std::move(source_config);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::AttributionScopesSetDataView,
                  attribution_reporting::AttributionScopesSet>::
    Read(attribution_reporting::mojom::AttributionScopesSetDataView data,
         attribution_reporting::AttributionScopesSet* out) {
  std::vector<std::string> scopes;
  if (!data.ReadScopes(&scopes)) {
    return false;
  }

  *out = attribution_reporting::AttributionScopesSet(std::move(scopes));
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::AttributionScopesDataDataView,
                  attribution_reporting::AttributionScopesData>::
    Read(attribution_reporting::mojom::AttributionScopesDataDataView data,
         attribution_reporting::AttributionScopesData* out) {
  attribution_reporting::AttributionScopesSet scopes;
  if (!data.ReadAttributionScopesSet(&scopes)) {
    return false;
  }

  auto attribution_scopes_data =
      attribution_reporting::AttributionScopesData::Create(
          std::move(scopes), data.attribution_scope_limit(),
          data.max_event_states());
  if (!attribution_scopes_data.has_value()) {
    return false;
  }

  *out = *std::move(attribution_scopes_data);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::SourceRegistrationDataView,
                  attribution_reporting::SourceRegistration>::
    Read(attribution_reporting::mojom::SourceRegistrationDataView data,
         attribution_reporting::SourceRegistration* out) {
  if (!data.ReadDestinations(&out->destination_set)) {
    return false;
  }

  if (!data.ReadExpiry(&out->expiry)) {
    return false;
  }

  if (!data.ReadAggregatableReportWindow(&out->aggregatable_report_window)) {
    return false;
  }

  if (!data.ReadTriggerSpecs(&out->trigger_specs)) {
    return false;
  }

  if (!data.ReadFilterData(&out->filter_data)) {
    return false;
  }

  if (!data.ReadAggregationKeys(&out->aggregation_keys)) {
    return false;
  }

  if (!data.ReadAttributionScopesData(&out->attribution_scopes_data)) {
    return false;
  }

  if (!out->event_level_epsilon.SetIfValid(data.event_level_epsilon())) {
    return false;
  }

  if (!data.ReadAggregatableDebugReportingConfig(
          &out->aggregatable_debug_reporting_config)) {
    return false;
  }

  out->source_event_id = data.source_event_id();
  out->priority = data.priority();
  out->debug_key = data.debug_key();
  out->debug_reporting = data.debug_reporting();
  out->trigger_data_matching = data.trigger_data_matching();
  out->destination_limit_priority = data.destination_limit_priority();
  return out->IsValid();
}

// static
bool StructTraits<attribution_reporting::mojom::FilterPairDataView,
                  attribution_reporting::FilterPair>::
    Read(attribution_reporting::mojom::FilterPairDataView data,
         attribution_reporting::FilterPair* out) {
  return data.ReadPositive(&out->positive) && data.ReadNegative(&out->negative);
}

// static
bool StructTraits<attribution_reporting::mojom::EventTriggerDataDataView,
                  attribution_reporting::EventTriggerData>::
    Read(attribution_reporting::mojom::EventTriggerDataDataView data,
         attribution_reporting::EventTriggerData* out) {
  if (!data.ReadFilters(&out->filters)) {
    return false;
  }

  out->dedup_key = data.dedup_key();
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

  std::vector<std::string> source_keys;
  if (!data.ReadSourceKeys(&source_keys)) {
    return false;
  }

  attribution_reporting::FilterPair filters;
  if (!data.ReadFilters(&filters)) {
    return false;
  }

  *out = attribution_reporting::AggregatableTriggerData(
      key_piece, std::move(source_keys), std::move(filters));
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::AggregatableDedupKeyDataView,
                  attribution_reporting::AggregatableDedupKey>::
    Read(attribution_reporting::mojom::AggregatableDedupKeyDataView data,
         attribution_reporting::AggregatableDedupKey* out) {
  if (!data.ReadFilters(&out->filters)) {
    return false;
  }

  out->dedup_key = data.dedup_key();
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::AggregatableValuesValueDataView,
                  attribution_reporting::AggregatableValuesValue>::
    Read(attribution_reporting::mojom::AggregatableValuesValueDataView data,
         attribution_reporting::AggregatableValuesValue* out) {
  auto aggregatable_values_value =
      attribution_reporting::AggregatableValuesValue::Create(
          data.value(), data.filtering_id());
  if (!aggregatable_values_value) {
    return false;
  }

  *out = *std::move(aggregatable_values_value);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::AggregatableValuesDataView,
                  attribution_reporting::AggregatableValues>::
    Read(attribution_reporting::mojom::AggregatableValuesDataView data,
         attribution_reporting::AggregatableValues* out) {
  attribution_reporting::FilterPair filters;
  if (!data.ReadFilters(&filters)) {
    return false;
  }

  attribution_reporting::AggregatableValues::Values values;
  if (!data.ReadValues(&values)) {
    return false;
  }
  auto aggregatable_values = attribution_reporting::AggregatableValues::Create(
      std::move(values), std::move(filters));
  if (!aggregatable_values) {
    return false;
  }

  *out = *std::move(aggregatable_values);
  return true;
}

// static
bool StructTraits<attribution_reporting::mojom::TriggerRegistrationDataView,
                  attribution_reporting::TriggerRegistration>::
    Read(attribution_reporting::mojom::TriggerRegistrationDataView data,
         attribution_reporting::TriggerRegistration* out) {
  if (!data.ReadEventTriggers(&out->event_triggers)) {
    return false;
  }

  if (!data.ReadFilters(&out->filters)) {
    return false;
  }

  if (!data.ReadAggregatableTriggerData(&out->aggregatable_trigger_data)) {
    return false;
  }

  if (!data.ReadAggregatableValues(&out->aggregatable_values)) {
    return false;
  }

  if (!data.ReadAggregatableDedupKeys(&out->aggregatable_dedup_keys)) {
    return false;
  }

  if (!data.ReadAggregationCoordinatorOrigin(
          &out->aggregation_coordinator_origin)) {
    return false;
  }

  std::optional<std::string> trigger_context_id;
  if (!data.ReadTriggerContextId(&trigger_context_id)) {
    return false;
  }

  auto max_bytes =
      attribution_reporting::AggregatableFilteringIdsMaxBytes::Create(
          data.aggregatable_filtering_id_max_bytes());
  if (!max_bytes.has_value()) {
    return false;
  }

  if (!data.ReadAttributionScopes(&out->attribution_scopes)) {
    return false;
  }

  std::optional<attribution_reporting::AggregatableTriggerConfig>
      aggregatable_trigger_config =
          attribution_reporting::AggregatableTriggerConfig::Create(
              data.source_registration_time_config(),
              std::move(trigger_context_id), max_bytes.value());
  if (!aggregatable_trigger_config.has_value()) {
    return false;
  }
  out->aggregatable_trigger_config = *std::move(aggregatable_trigger_config);

  if (!data.ReadAggregatableDebugReportingConfig(
          &out->aggregatable_debug_reporting_config)) {
    return false;
  }

  out->debug_key = data.debug_key();
  out->debug_reporting = data.debug_reporting();

  return out->IsValid();
}

// static
bool StructTraits<attribution_reporting::mojom::OsRegistrationItemDataView,
                  attribution_reporting::OsRegistrationItem>::
    Read(attribution_reporting::mojom::OsRegistrationItemDataView data,
         attribution_reporting::OsRegistrationItem* out) {
  if (!data.ReadUrl(&out->url)) {
    return false;
  }
  out->debug_reporting = data.debug_reporting();
  return true;
}

}  // namespace mojo
