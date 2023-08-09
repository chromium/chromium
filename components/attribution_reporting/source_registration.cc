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
#include "base/strings/string_number_conversions.h"
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
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
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
constexpr char kEventReportWindow[] = "event_report_window";
constexpr char kEventReportWindows[] = "event_report_windows";
constexpr char kExpiry[] = "expiry";
constexpr char kFilterData[] = "filter_data";
constexpr char kMaxEventLevelReports[] = "max_event_level_reports";
constexpr char kSourceEventId[] = "source_event_id";

bool ParseMaxEventLevelReports(const base::Value* value,
                               absl::optional<int>& out) {
  if (value) {
    out = value->GetIfInt();
    if (!out.has_value() || out.value() < 0 ||
        out.value() > kMaxSettableEventLevelAttributions) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool ParseTimeDeltaInSeconds(
    const base::Value& value,
    absl::optional<base::TimeDelta>& out) {
  // Note: The full range of uint64 seconds cannot be represented in the
  // resulting `base::TimeDelta`, but this is fine because `base::Seconds()`
  // properly clamps out-of-bound values and because the Attribution
  // Reporting API itself clamps values to 30 days:
  // https://wicg.github.io/attribution-reporting-api/#valid-source-expiry-range

  if (absl::optional<int> int_value = value.GetIfInt()) {
    if (*int_value < 0) {
      return false;
    }
    out = base::Seconds(*int_value);
    return true;
  }

  if (const std::string* str = value.GetIfString()) {
    uint64_t seconds;
    if (!base::StringToUint64(*str, &seconds)) {
      return false;
    }
    out = base::Seconds(seconds);
    return true;
  }

  return false;
}

[[nodiscard]] bool ParseTimeDeltaInSeconds(
    const base::Value::Dict& registration,
    base::StringPiece key,
    absl::optional<base::TimeDelta>& out) {
  out = absl::nullopt;

  const base::Value* value = registration.Find(key);
  if (!value) {
    return true;
  }
  return ParseTimeDeltaInSeconds(*value, out);
}

absl::optional<SourceRegistrationError> ParseEventReportWindowOrWindows(
    const base::Value::Dict& registration,
    SourceRegistration& result) {
  const base::Value* singular_window = registration.Find(kEventReportWindow);
  const base::Value* multiple_windows = registration.Find(kEventReportWindows);

  if (singular_window && multiple_windows) {
    return SourceRegistrationError::kBothEventReportWindowFieldsFound;
  } else if (singular_window) {
    if (!ParseTimeDeltaInSeconds(*singular_window,
                                 result.event_report_window)) {
      return SourceRegistrationError::kEventReportWindowValueInvalid;
    }
  } else if (multiple_windows) {
    base::expected<EventReportWindows, SourceRegistrationError>
        event_report_windows = EventReportWindows::FromJSON(*multiple_windows);

    if (!event_report_windows.has_value()) {
      return event_report_windows.error();
    }
    result.event_report_windows = std::move(event_report_windows.value());
  }
  return absl::nullopt;
}

void SerializeTimeDeltaInSeconds(base::Value::Dict& dict,
                                 base::StringPiece key,
                                 absl::optional<base::TimeDelta> value) {
  if (value) {
    int64_t seconds = value->InSeconds();
    if (base::IsValueInRangeForNumericType<int>(seconds)) {
      dict.Set(key, static_cast<int>(seconds));
    } else {
      SerializeInt64(dict, key, seconds);
    }
  }
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

  if (auto error = ParseEventReportWindowOrWindows(registration, result);
      error.has_value()) {
    return base::unexpected(error.value());
  }

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

  if (!ParseTimeDeltaInSeconds(registration, kExpiry, result.expiry)) {
    return base::unexpected(SourceRegistrationError::kExpiryValueInvalid);
  }

  if (!ParseTimeDeltaInSeconds(registration, kAggregatableReportWindow,
                               result.aggregatable_report_window)) {
    return base::unexpected(
        SourceRegistrationError::kAggregatableReportWindowValueInvalid);
  }

  result.debug_key = ParseDebugKey(registration);

  result.debug_reporting = ParseDebugReporting(registration);

  if (!ParseMaxEventLevelReports(registration.Find(kMaxEventLevelReports),
                                 result.max_event_level_reports)) {
    return base::unexpected(
        SourceRegistrationError::kMaxEventLevelReportsValueInvalid);
  }

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
  SerializeTimeDeltaInSeconds(dict, kEventReportWindow, event_report_window);
  SerializeTimeDeltaInSeconds(dict, kAggregatableReportWindow,
                              aggregatable_report_window);

  SerializeDebugKey(dict, debug_key);
  SerializeDebugReporting(dict, debug_reporting);

  if (event_report_windows.has_value()) {
    dict.Set(kEventReportWindows, event_report_windows->ToJson());
  }

  if (max_event_level_reports.has_value()) {
    dict.Set(kMaxEventLevelReports, max_event_level_reports.value());
  }

  return dict;
}

}  // namespace attribution_reporting
