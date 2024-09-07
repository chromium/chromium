// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
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
#include "url/gurl.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::SuitableOriginDataView,
                 attribution_reporting::SuitableOrigin> {
  static const url::Origin& origin(
      const attribution_reporting::SuitableOrigin& origin) {
    return *origin;
  }

  static bool Read(attribution_reporting::mojom::SuitableOriginDataView data,
                   attribution_reporting::SuitableOrigin* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::FilterDataDataView,
                 attribution_reporting::FilterData> {
  static const attribution_reporting::FilterValues& filter_values(
      const attribution_reporting::FilterData& filter_data) {
    return filter_data.filter_values();
  }

  static bool Read(attribution_reporting::mojom::FilterDataDataView data,
                   attribution_reporting::FilterData* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::FilterConfigDataView,
                 attribution_reporting::FilterConfig> {
  static const std::optional<base::TimeDelta>& lookback_window(
      const attribution_reporting::FilterConfig& filter_config) {
    return filter_config.lookback_window();
  }

  static const attribution_reporting::FilterValues& filter_values(
      const attribution_reporting::FilterConfig& filter_config) {
    return filter_config.filter_values();
  }

  static bool Read(attribution_reporting::mojom::FilterConfigDataView data,
                   attribution_reporting::FilterConfig* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AggregationKeysDataView,
                 attribution_reporting::AggregationKeys> {
  static const attribution_reporting::AggregationKeys::Keys& keys(
      const attribution_reporting::AggregationKeys& aggregation_keys) {
    return aggregation_keys.keys();
  }

  static bool Read(attribution_reporting::mojom::AggregationKeysDataView data,
                   attribution_reporting::AggregationKeys* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::DestinationSetDataView,
                 attribution_reporting::DestinationSet> {
  static const attribution_reporting::DestinationSet::Destinations&
  destinations(const attribution_reporting::DestinationSet& set) {
    return set.destinations();
  }

  static bool Read(attribution_reporting::mojom::DestinationSetDataView data,
                   attribution_reporting::DestinationSet* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::EventReportWindowsDataView,
                 attribution_reporting::EventReportWindows> {
  static base::TimeDelta start_time(
      const attribution_reporting::EventReportWindows& event_report_windows) {
    return event_report_windows.start_time();
  }

  static const base::flat_set<base::TimeDelta>& end_times(
      const attribution_reporting::EventReportWindows& event_report_windows) {
    return event_report_windows.end_times();
  }

  static bool Read(
      attribution_reporting::mojom::EventReportWindowsDataView data,
      attribution_reporting::EventReportWindows* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::TriggerSpecDataView,
                 attribution_reporting::TriggerSpec> {
  static const attribution_reporting::EventReportWindows& event_report_windows(
      const attribution_reporting::TriggerSpec& spec) {
    return spec.event_report_windows();
  }

  static bool Read(attribution_reporting::mojom::TriggerSpecDataView data,
                   attribution_reporting::TriggerSpec* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::TriggerSpecsDataView,
                 attribution_reporting::TriggerSpecs> {
  static const std::vector<attribution_reporting::TriggerSpec>& specs(
      const attribution_reporting::TriggerSpecs& specs) {
    return specs.specs();
  }

  static const attribution_reporting::TriggerSpecs::TriggerDataIndices&
  trigger_data_indices(const attribution_reporting::TriggerSpecs& specs) {
    return specs.trigger_data_indices();
  }

  static int max_event_level_reports(
      const attribution_reporting::TriggerSpecs& specs) {
    return specs.max_event_level_reports();
  }

  static bool Read(attribution_reporting::mojom::TriggerSpecsDataView data,
                   attribution_reporting::TriggerSpecs* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<
        attribution_reporting::mojom::
            AggregatableDebugReportingContributionDataView,
        attribution_reporting::AggregatableDebugReportingContribution> {
  static absl::uint128 key_piece(
      const attribution_reporting::AggregatableDebugReportingContribution&
          contribution) {
    return contribution.key_piece();
  }

  static uint32_t value(
      const attribution_reporting::AggregatableDebugReportingContribution&
          contribution) {
    return contribution.value();
  }

  static bool Read(
      attribution_reporting::mojom::
          AggregatableDebugReportingContributionDataView data,
      attribution_reporting::AggregatableDebugReportingContribution* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<
        attribution_reporting::mojom::AggregatableDebugReportingConfigDataView,
        attribution_reporting::AggregatableDebugReportingConfig> {
  static absl::uint128 key_piece(
      const attribution_reporting::AggregatableDebugReportingConfig& config) {
    return config.key_piece;
  }

  static const attribution_reporting::AggregatableDebugReportingConfig::
      DebugData&
      debug_data(const attribution_reporting::AggregatableDebugReportingConfig&
                     config) {
    return config.debug_data;
  }

  static const std::optional<attribution_reporting::SuitableOrigin>&
  aggregation_coordinator_origin(
      const attribution_reporting::AggregatableDebugReportingConfig& config) {
    return config.aggregation_coordinator_origin;
  }

  static bool Read(
      attribution_reporting::mojom::AggregatableDebugReportingConfigDataView
          data,
      attribution_reporting::AggregatableDebugReportingConfig* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<
        attribution_reporting::mojom::
            SourceAggregatableDebugReportingConfigDataView,
        attribution_reporting::SourceAggregatableDebugReportingConfig> {
  static uint32_t budget(
      const attribution_reporting::SourceAggregatableDebugReportingConfig&
          config) {
    return config.budget();
  }

  static const attribution_reporting::AggregatableDebugReportingConfig& config(
      const attribution_reporting::SourceAggregatableDebugReportingConfig&
          config) {
    return config.config();
  }

  static bool Read(
      attribution_reporting::mojom::
          SourceAggregatableDebugReportingConfigDataView data,
      attribution_reporting::SourceAggregatableDebugReportingConfig* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AttributionScopesSetDataView,
                 attribution_reporting::AttributionScopesSet> {
  static const attribution_reporting::AttributionScopesSet::Scopes& scopes(
      const attribution_reporting::AttributionScopesSet& set) {
    return set.scopes();
  }

  static bool Read(
      attribution_reporting::mojom::AttributionScopesSetDataView data,
      attribution_reporting::AttributionScopesSet* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AttributionScopesDataDataView,
                 attribution_reporting::AttributionScopesData> {
  static const attribution_reporting::AttributionScopesSet&
  attribution_scopes_set(
      const attribution_reporting::AttributionScopesData& data) {
    return data.attribution_scopes_set();
  }

  static uint32_t attribution_scope_limit(
      const attribution_reporting::AttributionScopesData& data) {
    return data.attribution_scope_limit();
  }

  static uint32_t max_event_states(
      const attribution_reporting::AttributionScopesData& data) {
    return data.max_event_states();
  }

  static bool Read(
      attribution_reporting::mojom::AttributionScopesDataDataView data,
      attribution_reporting::AttributionScopesData* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::SourceRegistrationDataView,
                 attribution_reporting::SourceRegistration> {
  static const attribution_reporting::DestinationSet& destinations(
      const attribution_reporting::SourceRegistration& source) {
    return source.destination_set;
  }

  static uint64_t source_event_id(
      const attribution_reporting::SourceRegistration& source) {
    return source.source_event_id;
  }

  static base::TimeDelta expiry(
      const attribution_reporting::SourceRegistration& source) {
    return source.expiry;
  }

  static base::TimeDelta aggregatable_report_window(
      const attribution_reporting::SourceRegistration& source) {
    return source.aggregatable_report_window;
  }

  static const attribution_reporting::TriggerSpecs& trigger_specs(
      const attribution_reporting::SourceRegistration& source) {
    return source.trigger_specs;
  }

  static int64_t priority(
      const attribution_reporting::SourceRegistration& source) {
    return source.priority;
  }

  static std::optional<uint64_t> debug_key(
      const attribution_reporting::SourceRegistration& source) {
    return source.debug_key;
  }

  static const attribution_reporting::FilterData& filter_data(
      const attribution_reporting::SourceRegistration& source) {
    return source.filter_data;
  }

  static const attribution_reporting::AggregationKeys& aggregation_keys(
      const attribution_reporting::SourceRegistration& source) {
    return source.aggregation_keys;
  }

  static bool debug_reporting(
      const attribution_reporting::SourceRegistration& source) {
    return source.debug_reporting;
  }

  static attribution_reporting::mojom::TriggerDataMatching
  trigger_data_matching(
      const attribution_reporting::SourceRegistration& source) {
    return source.trigger_data_matching;
  }

  static double event_level_epsilon(
      const attribution_reporting::SourceRegistration& source) {
    return source.event_level_epsilon;
  }

  static const attribution_reporting::SourceAggregatableDebugReportingConfig&
  aggregatable_debug_reporting_config(
      const attribution_reporting::SourceRegistration& source) {
    return source.aggregatable_debug_reporting_config;
  }

  static int64_t destination_limit_priority(
      const attribution_reporting::SourceRegistration& source) {
    return source.destination_limit_priority;
  }

  static const std::optional<attribution_reporting::AttributionScopesData>&
  attribution_scopes_data(
      const attribution_reporting::SourceRegistration& source) {
    return source.attribution_scopes_data;
  }

  static bool Read(
      attribution_reporting::mojom::SourceRegistrationDataView data,
      attribution_reporting::SourceRegistration* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::FilterPairDataView,
                 attribution_reporting::FilterPair> {
  static const attribution_reporting::FiltersDisjunction& positive(
      const attribution_reporting::FilterPair& filters) {
    return filters.positive;
  }

  static const attribution_reporting::FiltersDisjunction& negative(
      const attribution_reporting::FilterPair& filters) {
    return filters.negative;
  }

  static bool Read(attribution_reporting::mojom::FilterPairDataView data,
                   attribution_reporting::FilterPair* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::EventTriggerDataDataView,
                 attribution_reporting::EventTriggerData> {
  static uint64_t data(const attribution_reporting::EventTriggerData& data) {
    return data.data;
  }

  static int64_t priority(const attribution_reporting::EventTriggerData& data) {
    return data.priority;
  }

  static std::optional<uint64_t> dedup_key(
      const attribution_reporting::EventTriggerData& data) {
    return data.dedup_key;
  }

  static const attribution_reporting::FilterPair& filters(
      const attribution_reporting::EventTriggerData& data) {
    return data.filters;
  }

  static bool Read(attribution_reporting::mojom::EventTriggerDataDataView data,
                   attribution_reporting::EventTriggerData* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AggregatableTriggerDataDataView,
                 attribution_reporting::AggregatableTriggerData> {
  static absl::uint128 key_piece(
      const attribution_reporting::AggregatableTriggerData& data) {
    return data.key_piece();
  }

  static const attribution_reporting::AggregatableTriggerData::Keys&
  source_keys(const attribution_reporting::AggregatableTriggerData& data) {
    return data.source_keys();
  }

  static const attribution_reporting::FilterPair& filters(
      const attribution_reporting::AggregatableTriggerData& data) {
    return data.filters();
  }

  static bool Read(
      attribution_reporting::mojom::AggregatableTriggerDataDataView data,
      attribution_reporting::AggregatableTriggerData* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::TriggerRegistrationDataView,
                 attribution_reporting::TriggerRegistration> {
  static const std::vector<attribution_reporting::EventTriggerData>&
  event_triggers(const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.event_triggers;
  }

  static const attribution_reporting::FilterPair& filters(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.filters;
  }

  static const std::vector<attribution_reporting::AggregatableTriggerData>&
  aggregatable_trigger_data(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_trigger_data;
  }

  static const std::vector<attribution_reporting::AggregatableValues>&
  aggregatable_values(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_values;
  }

  static std::optional<uint64_t> debug_key(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.debug_key;
  }

  static const std::vector<attribution_reporting::AggregatableDedupKey>&
  aggregatable_dedup_keys(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_dedup_keys;
  }

  static bool debug_reporting(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.debug_reporting;
  }

  static const std::optional<attribution_reporting::SuitableOrigin>&
  aggregation_coordinator_origin(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregation_coordinator_origin;
  }

  static attribution_reporting::mojom::SourceRegistrationTimeConfig
  source_registration_time_config(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_trigger_config
        .source_registration_time_config();
  }

  static const std::optional<std::string>& trigger_context_id(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_trigger_config.trigger_context_id();
  }

  static uint8_t aggregatable_filtering_id_max_bytes(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_trigger_config
        .aggregatable_filtering_id_max_bytes()
        .value();
  }

  static const attribution_reporting::AggregatableDebugReportingConfig&
  aggregatable_debug_reporting_config(
      const attribution_reporting::TriggerRegistration& source) {
    return source.aggregatable_debug_reporting_config;
  }

  static const attribution_reporting::AttributionScopesSet& attribution_scopes(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.attribution_scopes;
  }

  static bool Read(
      attribution_reporting::mojom::TriggerRegistrationDataView data,
      attribution_reporting::TriggerRegistration* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AggregatableDedupKeyDataView,
                 attribution_reporting::AggregatableDedupKey> {
  static std::optional<uint64_t> dedup_key(
      const attribution_reporting::AggregatableDedupKey& data) {
    return data.dedup_key;
  }

  static const attribution_reporting::FilterPair& filters(
      const attribution_reporting::AggregatableDedupKey& data) {
    return data.filters;
  }

  static bool Read(
      attribution_reporting::mojom::AggregatableDedupKeyDataView data,
      attribution_reporting::AggregatableDedupKey* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AggregatableValuesValueDataView,
                 attribution_reporting::AggregatableValuesValue> {
  static uint32_t value(
      const attribution_reporting::AggregatableValuesValue& data) {
    return data.value();
  }

  static uint64_t filtering_id(
      const attribution_reporting::AggregatableValuesValue& data) {
    return data.filtering_id();
  }

  static bool Read(
      attribution_reporting::mojom::AggregatableValuesValueDataView data,
      attribution_reporting::AggregatableValuesValue* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AggregatableValuesDataView,
                 attribution_reporting::AggregatableValues> {
  static const attribution_reporting::AggregatableValues::Values& values(
      const attribution_reporting::AggregatableValues& data) {
    return data.values();
  }

  static const attribution_reporting::FilterPair& filters(
      const attribution_reporting::AggregatableValues& data) {
    return data.filters();
  }

  static bool Read(
      attribution_reporting::mojom::AggregatableValuesDataView data,
      attribution_reporting::AggregatableValues* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::OsRegistrationDataView,
                 std::vector<attribution_reporting::OsRegistrationItem>> {
  static const std::vector<attribution_reporting::OsRegistrationItem>& items(
      const std::vector<attribution_reporting::OsRegistrationItem>& items) {
    return items;
  }

  static bool Read(
      attribution_reporting::mojom::OsRegistrationDataView data,
      std::vector<attribution_reporting::OsRegistrationItem>* out) {
    return data.ReadItems(out);
  }
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::OsRegistrationItemDataView,
                 attribution_reporting::OsRegistrationItem> {
  static const GURL& url(
      const attribution_reporting::OsRegistrationItem& item) {
    return item.url;
  }

  static bool debug_reporting(
      const attribution_reporting::OsRegistrationItem& item) {
    return item.debug_reporting;
  }

  static bool Read(
      attribution_reporting::mojom::OsRegistrationItemDataView data,
      attribution_reporting::OsRegistrationItem* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS_H_
