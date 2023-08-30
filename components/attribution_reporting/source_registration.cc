// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

constexpr char kAggregatableReportWindow[] = "aggregatable_report_window";
constexpr char kAggregationKeys[] = "aggregation_keys";
constexpr char kDestination[] = "destination";
constexpr char kExpiry[] = "expiry";
constexpr char kFilterData[] = "filter_data";
constexpr char kMaxEventLevelReports[] = "max_event_level_reports";
constexpr char kSourceEventId[] = "source_event_id";

base::expected<int, SourceRegistrationError> ParseMaxEventLevelReports(
    const base::Value& value) {
  absl::optional<int> i = value.GetIfInt();
  if (!i.has_value() || *i < 0 || *i > kMaxSettableEventLevelAttributions) {
    return base::unexpected(
        SourceRegistrationError::kMaxEventLevelReportsValueInvalid);
  }

  return *i;
}

}  // namespace

void RecordSourceRegistrationError(mojom::SourceRegistrationError error) {
  base::UmaHistogramEnumeration("Conversions.SourceRegistrationError5", error);
}

SourceRegistration::SourceRegistration(mojo::DefaultConstruct::Tag tag)
    : destination_set(tag) {}

SourceRegistration::SourceRegistration(DestinationSet destination_set)
    : destination_set(std::move(destination_set)) {}

SourceRegistration::~SourceRegistration() = default;

SourceRegistration::SourceRegistration(const SourceRegistration&) = default;

SourceRegistration& SourceRegistration::operator=(const SourceRegistration&) =
    default;

SourceRegistration::SourceRegistration(SourceRegistration&&) = default;

SourceRegistration& SourceRegistration::operator=(SourceRegistration&&) =
    default;

// static
base::expected<SourceRegistration, SourceRegistrationError>
SourceRegistration::Parse(base::Value::Dict registration) {
  ASSIGN_OR_RETURN(DestinationSet destination_set,
                   DestinationSet::FromJSON(registration.Find(kDestination)));
  SourceRegistration result(std::move(destination_set));

  ASSIGN_OR_RETURN(result.filter_data,
                   FilterData::FromJSON(registration.Find(kFilterData)));

  ASSIGN_OR_RETURN(result.event_report_windows,
                   EventReportWindows::FromJSON(registration));

  ASSIGN_OR_RETURN(
      result.aggregation_keys,
      AggregationKeys::FromJSON(registration.Find(kAggregationKeys)));

  absl::optional<uint64_t> source_event_id;
  if (!ParseUint64(registration, kSourceEventId, source_event_id)) {
    return base::unexpected(
        SourceRegistrationError::kSourceEventIdValueInvalid);
  }
  result.source_event_id = source_event_id.value_or(0);

  absl::optional<int64_t> priority;
  if (!ParsePriority(registration, priority)) {
    return base::unexpected(SourceRegistrationError::kPriorityValueInvalid);
  }
  result.priority = priority.value_or(0);

  if (const base::Value* value = registration.Find(kExpiry)) {
    ASSIGN_OR_RETURN(result.expiry,
                     ParseLegacyDuration(
                         *value, SourceRegistrationError::kExpiryValueInvalid));
  }

  if (const base::Value* value = registration.Find(kAggregatableReportWindow)) {
    ASSIGN_OR_RETURN(
        result.aggregatable_report_window,
        ParseLegacyDuration(
            *value,
            SourceRegistrationError::kAggregatableReportWindowValueInvalid));
  }

  if (const base::Value* value = registration.Find(kMaxEventLevelReports)) {
    ASSIGN_OR_RETURN(result.max_event_level_reports,
                     ParseMaxEventLevelReports(*value));
  }

  result.debug_key = ParseDebugKey(registration);

  result.debug_reporting = ParseDebugReporting(registration);

  return result;
}

// static
base::expected<SourceRegistration, SourceRegistrationError>
SourceRegistration::Parse(base::StringPiece json) {
  base::expected<SourceRegistration, SourceRegistrationError> source =
      base::unexpected(SourceRegistrationError::kInvalidJson);

  absl::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);

  if (value) {
    if (value->is_dict()) {
      source = Parse(std::move(*value).TakeDict());
    } else {
      source = base::unexpected(SourceRegistrationError::kRootWrongType);
    }
  }

  if (!source.has_value()) {
    RecordSourceRegistrationError(source.error());
  }

  return source;
}

base::Value::Dict SourceRegistration::ToJson() const {
  base::Value::Dict dict;

  dict.Set(kDestination, destination_set.ToJson());

  if (!filter_data.filter_values().empty()) {
    dict.Set(kFilterData, filter_data.ToJson());
  }

  if (!aggregation_keys.keys().empty()) {
    dict.Set(kAggregationKeys, aggregation_keys.ToJson());
  }

  SerializeUint64(dict, kSourceEventId, source_event_id);
  SerializePriority(dict, priority);

  SerializeTimeDeltaInSeconds(dict, kExpiry, expiry);

  if (event_report_windows.has_value()) {
    event_report_windows->Serialize(dict);
  }

  SerializeTimeDeltaInSeconds(dict, kAggregatableReportWindow,
                              aggregatable_report_window);

  SerializeDebugKey(dict, debug_key);
  SerializeDebugReporting(dict, debug_reporting);

  if (max_event_level_reports.has_value()) {
    dict.Set(kMaxEventLevelReports, max_event_level_reports.value());
  }

  return dict;
}

}  // namespace attribution_reporting
