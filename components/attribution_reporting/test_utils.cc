// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/test_utils.h"

#include <ostream>
#include <tuple>

#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_attestation.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace attribution_reporting {

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

std::ostream& operator<<(std::ostream& out, const FilterData& filter_data) {
  return out << filter_data.ToJson();
}

bool operator==(const Filters& a, const Filters& b) {
  return a.filter_values() == b.filter_values();
}

std::ostream& operator<<(std::ostream& out, const Filters& filters) {
  return out << filters.ToJson();
}

bool operator==(const SourceRegistration& a, const SourceRegistration& b) {
  auto tie = [](const SourceRegistration& s) {
    return std::make_tuple(s.source_event_id, s.destination, s.expiry,
                           s.event_report_window, s.aggregatable_report_window,
                           s.priority, s.filter_data, s.debug_key,
                           s.aggregation_keys, s.debug_reporting);
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
                           trigger_data.filters(), trigger_data.not_filters());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const AggregatableTriggerData& trigger_data) {
  return out << trigger_data.ToJson();
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
  return out << event_trigger.ToJson();
}

bool operator==(const TriggerRegistration& a, const TriggerRegistration& b) {
  auto tie = [](const TriggerRegistration& reg) {
    return std::make_tuple(reg.debug_key, reg.aggregatable_dedup_key,
                           reg.event_triggers, reg.aggregatable_trigger_data,
                           reg.aggregatable_values, reg.debug_reporting,
                           reg.aggregation_coordinator);
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

bool operator==(const TriggerAttestation& a, const TriggerAttestation& b) {
  auto tie = [](const TriggerAttestation& t) {
    return std::make_tuple(t.token(), t.aggregatable_report_id());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const TriggerAttestation& attestation) {
  return out << "{token=" << attestation.token() << ",aggregatable_report_id="
             << attestation.aggregatable_report_id() << "}";
}

}  // namespace attribution_reporting
