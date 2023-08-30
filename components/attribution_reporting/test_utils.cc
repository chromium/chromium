// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/test_utils.h"

#include <ostream>
#include <string>
#include <tuple>

#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace attribution_reporting {

FiltersDisjunction FiltersForSourceType(
    mojom::SourceType source_type,
    absl::optional<base::TimeDelta> lookback_window) {
  return {*FilterConfig::Create(
      {
          {
              {FilterData::kSourceTypeFilterKey, {SourceTypeName(source_type)}},
          },
      },
      lookback_window)};
}

bool operator==(const AggregationKeys& a, const AggregationKeys& b) {
  return a.keys() == b.keys();
}

std::ostream& operator<<(std::ostream& out,
                         const AggregationKeys& aggregation_keys) {
  return out << aggregation_keys.ToJson();
}

bool operator==(const FilterData& a, const FilterData& b) {
  return a.filter_values() == b.filter_values();
}

bool operator==(const FilterConfig& a, const FilterConfig& b) {
  auto tie = [](const FilterConfig& c) {
    return std::make_tuple(c.filter_values(), c.lookback_window());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const FilterData& filter_data) {
  return out << filter_data.ToJson();
}

bool operator==(const FilterPair& a, const FilterPair& b) {
  return a.positive == b.positive && a.negative == b.negative;
}

std::ostream& operator<<(std::ostream& out, const FilterPair& filters) {
  base::Value::Dict dict;
  filters.SerializeIfNotEmpty(dict);
  return out << dict;
}

bool operator==(const DestinationSet& a, const DestinationSet& b) {
  return a.destinations() == b.destinations();
}

std::ostream& operator<<(std::ostream& out,
                         const DestinationSet& destination_set) {
  return out << destination_set.ToJson();
}

bool operator==(const EventReportWindows& a, const EventReportWindows& b) {
  return a.start_time_or_window_time() == b.start_time_or_window_time() &&
         a.end_times() == b.end_times();
}

std::ostream& operator<<(std::ostream& out,
                         const EventReportWindows& event_report_windows) {
  base::Value::Dict dict;
  event_report_windows.Serialize(dict);
  return out << dict;
}

bool operator==(const SourceRegistration& a, const SourceRegistration& b) {
  auto tie = [](const SourceRegistration& s) {
    return std::make_tuple(
        s.source_event_id, s.destination_set, s.expiry, s.event_report_windows,
        s.aggregatable_report_window, s.priority, s.filter_data, s.debug_key,
        s.aggregation_keys, s.debug_reporting, s.max_event_level_reports);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const SourceRegistration& s) {
  return out << s.ToJson();
}

bool operator==(const AggregatableValues& a, const AggregatableValues& b) {
  return a.values() == b.values();
}

std::ostream& operator<<(std::ostream& out, const AggregatableValues& values) {
  return out << values.ToJson();
}

bool operator==(const AggregatableTriggerData& a,
                const AggregatableTriggerData& b) {
  const auto tie = [](const AggregatableTriggerData& trigger_data) {
    return std::make_tuple(trigger_data.key_piece(), trigger_data.source_keys(),
                           trigger_data.filters());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const AggregatableTriggerData& trigger_data) {
  return out << trigger_data.ToJson();
}

bool operator==(const EventTriggerData& a, const EventTriggerData& b) {
  const auto tie = [](const EventTriggerData& t) {
    return std::make_tuple(t.data, t.priority, t.dedup_key, t.filters);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const EventTriggerData& event_trigger) {
  return out << event_trigger.ToJson();
}

bool operator==(const TriggerRegistration& a, const TriggerRegistration& b) {
  auto tie = [](const TriggerRegistration& reg) {
    return std::make_tuple(reg.filters, reg.debug_key,
                           reg.aggregatable_dedup_keys, reg.event_triggers,
                           reg.aggregatable_trigger_data,
                           reg.aggregatable_values, reg.debug_reporting,
                           reg.aggregation_coordinator_origin,
                           reg.source_registration_time_config);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const TriggerRegistration& reg) {
  return out << reg.ToJson();
}

bool operator==(const SuitableOrigin& a, const SuitableOrigin& b) {
  return *a == *b;
}

std::ostream& operator<<(std::ostream& out, const SuitableOrigin& origin) {
  return out << *origin;
}

bool operator==(const AggregatableDedupKey& a, const AggregatableDedupKey& b) {
  const auto tie = [](const AggregatableDedupKey& t) {
    return std::make_tuple(t.dedup_key, t.filters);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const AggregatableDedupKey& aggregatable_dedup_key) {
  return out << aggregatable_dedup_key.ToJson();
}

bool operator==(const OsRegistrationItem& a, const OsRegistrationItem& b) {
  return std::tie(a.url, a.debug_reporting) ==
         std::tie(b.url, b.debug_reporting);
}

std::ostream& operator<<(std::ostream& out, const OsRegistrationItem& item) {
  return out << "{url=" << item.url
             << ", debug_reporting=" << item.debug_reporting << "}";
}

}  // namespace attribution_reporting
