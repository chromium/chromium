// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

absl::optional<base::TimeDelta> ParseTimeDeltaInSeconds(
    const base::Value::Dict& registration,
    base::StringPiece key) {
  if (absl::optional<int64_t> seconds = ParseInt64(registration, key))
    return base::Seconds(*seconds);
  return absl::nullopt;
}

base::expected<SuitableOrigin, SourceRegistrationError> ParseDestination(
    const base::Value::Dict& registration) {
  const base::Value* v = registration.Find("destination");
  if (!v)
    return base::unexpected(SourceRegistrationError::kDestinationMissing);

  const std::string* s = v->GetIfString();
  if (!s)
    return base::unexpected(SourceRegistrationError::kDestinationWrongType);

  auto destination = SuitableOrigin::Deserialize(*s);
  if (!destination.has_value())
    return base::unexpected(SourceRegistrationError::kDestinationUntrustworthy);

  return *destination;
}

}  // namespace

SourceRegistration::SourceRegistration(SuitableOrigin destination,
                                       SuitableOrigin reporting_origin)
    : destination(std::move(destination)),
      reporting_origin(std::move(reporting_origin)) {}

SourceRegistration::~SourceRegistration() = default;

SourceRegistration::SourceRegistration(const SourceRegistration&) = default;

SourceRegistration& SourceRegistration::operator=(const SourceRegistration&) =
    default;

SourceRegistration::SourceRegistration(SourceRegistration&&) = default;

SourceRegistration& SourceRegistration::operator=(SourceRegistration&&) =
    default;

// static
base::expected<SourceRegistration, SourceRegistrationError>
SourceRegistration::Parse(base::Value::Dict registration,
                          SuitableOrigin reporting_origin) {
  auto destination = ParseDestination(registration);
  if (!destination.has_value())
    return base::unexpected(destination.error());

  SourceRegistration result(std::move(*destination),
                            std::move(reporting_origin));

  base::expected<FilterData, SourceRegistrationError> filter_data =
      FilterData::FromJSON(registration.Find("filter_data"));
  if (!filter_data.has_value())
    return base::unexpected(filter_data.error());

  result.filter_data = std::move(*filter_data);

  base::expected<AggregationKeys, SourceRegistrationError> aggregation_keys =
      AggregationKeys::FromJSON(registration.Find("aggregation_keys"));
  if (!aggregation_keys.has_value())
    return base::unexpected(aggregation_keys.error());

  result.aggregation_keys = std::move(*aggregation_keys);

  result.source_event_id =
      ParseUint64(registration, "source_event_id").value_or(0);

  result.priority = ParsePriority(registration);

  result.expiry = ParseTimeDeltaInSeconds(registration, "expiry");

  result.event_report_window =
      ParseTimeDeltaInSeconds(registration, "event_report_window");

  result.aggregatable_report_window =
      ParseTimeDeltaInSeconds(registration, "aggregatable_report_window");

  result.debug_key = ParseUint64(registration, "debug_key");

  result.debug_reporting =
      registration.FindBool("debug_reporting").value_or(false);

  return result;
}

}  // namespace attribution_reporting
