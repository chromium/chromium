// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/test_utils.h"

#include <ostream>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
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

std::ostream& operator<<(std::ostream& out,
                         const AggregationKeys& aggregation_keys) {
  return out << aggregation_keys.ToJson();
}

std::ostream& operator<<(std::ostream& out, const FilterData& filter_data) {
  return out << filter_data.ToJson();
}

std::ostream& operator<<(std::ostream& out, const FilterPair& filters) {
  base::Value::Dict dict;
  filters.SerializeIfNotEmpty(dict);
  return out << dict;
}

std::ostream& operator<<(std::ostream& out,
                         const DestinationSet& destination_set) {
  return out << destination_set.ToJson();
}

std::ostream& operator<<(std::ostream& out,
                         const EventReportWindows& event_report_windows) {
  base::Value::Dict dict;
  event_report_windows.Serialize(dict);
  return out << dict;
}

std::ostream& operator<<(std::ostream& out, const SourceRegistration& s) {
  return out << s.ToJson();
}

std::ostream& operator<<(std::ostream& out, const AggregatableValues& values) {
  return out << values.ToJson();
}

std::ostream& operator<<(std::ostream& out,
                         const AggregatableTriggerData& trigger_data) {
  return out << trigger_data.ToJson();
}

std::ostream& operator<<(std::ostream& out,
                         const EventTriggerData& event_trigger) {
  return out << event_trigger.ToJson();
}

std::ostream& operator<<(std::ostream& out, const TriggerRegistration& reg) {
  return out << reg.ToJson();
}

std::ostream& operator<<(std::ostream& out, const SuitableOrigin& origin) {
  return out << *origin;
}

std::ostream& operator<<(std::ostream& out,
                         const AggregatableDedupKey& aggregatable_dedup_key) {
  return out << aggregatable_dedup_key.ToJson();
}

std::ostream& operator<<(std::ostream& out, const OsRegistrationItem& item) {
  return out << "{url=" << item.url
             << ", debug_reporting=" << item.debug_reporting << "}";
}

std::ostream& operator<<(std::ostream& out, const SummaryBuckets& buckets) {
  base::Value::Dict dict;
  buckets.Serialize(dict);
  return out << dict;
}

std::ostream& operator<<(std::ostream& out, const TriggerSpec& spec) {
  return out << spec.ToJson();
}

std::ostream& operator<<(std::ostream& out, const TriggerSpecs& specs) {
  return out << specs.ToJson();
}

std::ostream& operator<<(std::ostream& out,
                         const TriggerSpecs::const_iterator& it) {
  if (!it) {
    return out << "(end)";
  }
  return out << "{" << (*it).first << ", " << (*it).second << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AggregatableTriggerConfig& aggregatable_trigger_config) {
  base::Value::Dict dict;
  aggregatable_trigger_config.Serialize(dict);
  return out << dict;
}

}  // namespace attribution_reporting
