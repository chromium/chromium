// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/test_utils.h"

#include <ostream>
#include <tuple>

#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

std::ostream& WriteFilterValues(std::ostream& out,
                                const FilterValues& filter_values) {
  out << "{";

  const char* outer_separator = "";
  for (const auto& [filter, values] : filter_values) {
    out << outer_separator << filter << "=[";

    const char* inner_separator = "";
    for (const auto& value : values) {
      out << inner_separator << value;
      inner_separator = ", ";
    }

    out << "]";
    outer_separator = ", ";
  }

  return out << "}";
}

template <typename T>
std::ostream& WriteOptional(std::ostream& out, const absl::optional<T>& value) {
  if (value)
    return out << *value;

  return out << "null";
}

}  // namespace

bool operator==(const AggregationKeys& a, const AggregationKeys& b) {
  return a.keys() == b.keys();
}

std::ostream& operator<<(std::ostream& out,
                         const AggregationKeys& aggregation_keys) {
  out << "{";

  const char* separator = "";
  for (const auto& [key_id, key] : aggregation_keys.keys()) {
    out << separator << key_id << ":" << key;
    separator = ", ";
  }
  return out << "}";
}

bool operator==(const FilterData& a, const FilterData& b) {
  return a.filter_values() == b.filter_values();
}

std::ostream& operator<<(std::ostream& out, const FilterData& filter_data) {
  return WriteFilterValues(out, filter_data.filter_values());
}

bool operator==(const Filters& a, const Filters& b) {
  return a.filter_values() == b.filter_values();
}

std::ostream& operator<<(std::ostream& out, const Filters& filters) {
  return WriteFilterValues(out, filters.filter_values());
}

bool operator==(const SourceRegistration& a, const SourceRegistration& b) {
  auto tie = [](const SourceRegistration& s) {
    return std::make_tuple(
        s.source_event_id(), s.destination(), s.reporting_origin(), s.expiry(),
        s.event_report_window(), s.aggregatable_report_window(), s.priority(),
        s.filter_data(), s.debug_key(), s.aggregation_keys(),
        s.debug_reporting());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const SourceRegistration& s) {
  out << "{source_event_id=" << s.source_event_id()
      << ",destination=" << s.destination()
      << ",reporting_origin=" << s.reporting_origin() << ",expiry=";
  WriteOptional(out, s.expiry()) << ",event_report_window=";
  WriteOptional(out, s.event_report_window()) << ",aggregatable_report_window=";
  WriteOptional(out, s.aggregatable_report_window())
      << ",priority=" << s.priority() << ",filter_data=" << s.filter_data()
      << ",debug_key=";
  WriteOptional(out, s.debug_key())
      << ",aggregation_keys=" << s.aggregation_keys()
      << ",debug_reporting=" << s.debug_reporting() << "}";
  return out;
}

bool operator==(const AggregatableValues& a, const AggregatableValues& b) {
  return a.values() == b.values();
}

std::ostream& operator<<(std::ostream& out, const AggregatableValues& values) {
  out << "{";
  const char* separator = "";
  for (const auto& [key, value] : values.values()) {
    out << separator << key << ":" << value;
    separator = ", ";
  }
  return out << "}";
}

bool operator==(const AggregatableTriggerData& a,
                const AggregatableTriggerData& b) {
  const auto tie = [](const AggregatableTriggerData& trigger_data) {
    return std::make_tuple(trigger_data.key_piece(), trigger_data.source_keys(),
                           trigger_data.filters(), trigger_data.not_filters());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const AggregatableTriggerData& trigger_data) {
  out << "{key_piece=" << trigger_data.key_piece() << ",source_keys=[";

  const char* separator = "";
  for (const auto& key : trigger_data.source_keys()) {
    out << separator << key;
    separator = ", ";
  }

  return out << "],filters=" << trigger_data.filters()
             << ",not_filters=" << trigger_data.not_filters() << "}";
}

bool operator==(const EventTriggerData& a, const EventTriggerData& b) {
  const auto tie = [](const EventTriggerData& t) {
    return std::make_tuple(t.data, t.priority, t.dedup_key, t.filters,
                           t.not_filters);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const EventTriggerData& event_trigger) {
  out << "{data=" << event_trigger.data
      << ",priority=" << event_trigger.priority << ",dedup_key=";
  WriteOptional(out, event_trigger.dedup_key)
      << ",filters=" << event_trigger.filters
      << ",not_filters=" << event_trigger.not_filters << "}";
  return out;
}

}  // namespace attribution_reporting
