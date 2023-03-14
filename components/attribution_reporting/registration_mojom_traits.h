// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom-shared.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/registration.mojom-shared.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "mojo/public/cpp/base/int128_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::DebugKeyDataView, uint64_t> {
  static uint64_t value(uint64_t debug_key) { return debug_key; }

  static bool Read(attribution_reporting::mojom::DebugKeyDataView data,
                   uint64_t* out) {
    *out = data.value();
    return true;
  }
};

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
    StructTraits<attribution_reporting::mojom::TriggerDedupKeyDataView,
                 uint64_t> {
  static uint64_t value(uint64_t debug_key) { return debug_key; }

  static bool Read(attribution_reporting::mojom::TriggerDedupKeyDataView data,
                   uint64_t* out) {
    *out = data.value();
    return true;
  }
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

  static absl::optional<base::TimeDelta> expiry(
      const attribution_reporting::SourceRegistration& source) {
    return source.expiry;
  }

  static absl::optional<base::TimeDelta> event_report_window(
      const attribution_reporting::SourceRegistration& source) {
    return source.event_report_window;
  }

  static absl::optional<base::TimeDelta> aggregatable_report_window(
      const attribution_reporting::SourceRegistration& source) {
    return source.aggregatable_report_window;
  }

  static int64_t priority(
      const attribution_reporting::SourceRegistration& source) {
    return source.priority;
  }

  static absl::optional<uint64_t> debug_key(
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

  static absl::optional<uint64_t> dedup_key(
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

  static const attribution_reporting::AggregatableValues::Values&
  aggregatable_values(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregatable_values.values();
  }

  static absl::optional<uint64_t> debug_key(
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

  static aggregation_service::mojom::AggregationCoordinator
  aggregation_coordinator(
      const attribution_reporting::TriggerRegistration& trigger) {
    return trigger.aggregation_coordinator;
  }

  static bool Read(
      attribution_reporting::mojom::TriggerRegistrationDataView data,
      attribution_reporting::TriggerRegistration* out);
};

template <>
struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::AggregatableDedupKeyDataView,
                 attribution_reporting::AggregatableDedupKey> {
  static absl::optional<uint64_t> dedup_key(
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

}  // namespace mojo

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_MOJOM_TRAITS_H_
