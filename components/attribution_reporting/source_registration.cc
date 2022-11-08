// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

absl::optional<uint64_t> ParseUint64(const base::Value::Dict& dict,
                                     base::StringPiece key) {
  const std::string* s = dict.FindString(key);
  if (!s)
    return absl::nullopt;

  uint64_t value;
  return base::StringToUint64(*s, &value) ? absl::make_optional(value)
                                          : absl::nullopt;
}

absl::optional<int64_t> ParseInt64(const base::Value::Dict& dict,
                                   base::StringPiece key) {
  const std::string* s = dict.FindString(key);
  if (!s)
    return absl::nullopt;

  int64_t value;
  return base::StringToInt64(*s, &value) ? absl::make_optional(value)
                                         : absl::nullopt;
}

absl::optional<base::TimeDelta> ParseTimeDeltaInSeconds(
    const base::Value::Dict& registration,
    base::StringPiece key) {
  if (absl::optional<int64_t> seconds = ParseInt64(registration, key))
    return base::Seconds(*seconds);
  return absl::nullopt;
}

}  // namespace

SourceRegistration::SourceRegistration() = default;

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
                          url::Origin reporting_origin) {
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  SourceRegistration result;

  {
    const base::Value* v = registration.Find("destination");
    if (!v)
      return base::unexpected(SourceRegistrationError::kDestinationMissing);

    const std::string* s = v->GetIfString();
    if (!s)
      return base::unexpected(SourceRegistrationError::kDestinationWrongType);

    result.destination_ = url::Origin::Create(GURL(*s));
    if (!network::IsOriginPotentiallyTrustworthy(result.destination_)) {
      return base::unexpected(
          SourceRegistrationError::kDestinationUntrustworthy);
    }
  }

  result.source_event_id_ =
      ParseUint64(registration, "source_event_id").value_or(0);

  result.priority_ = ParseInt64(registration, "priority").value_or(0);

  result.expiry_ = ParseTimeDeltaInSeconds(registration, "expiry");

  result.event_report_window_ =
      ParseTimeDeltaInSeconds(registration, "event_report_window");

  result.aggregatable_report_window_ =
      ParseTimeDeltaInSeconds(registration, "aggregatable_report_window");

  result.debug_key_ = ParseUint64(registration, "debug_key");

  base::expected<FilterData, SourceRegistrationError> filter_data =
      FilterData::FromJSON(registration.Find("filter_data"));
  if (!filter_data.has_value())
    return base::unexpected(filter_data.error());

  result.filter_data_ = std::move(*filter_data);

  base::expected<AggregationKeys, SourceRegistrationError> aggregation_keys =
      AggregationKeys::FromJSON(registration.Find("aggregation_keys"));
  if (!aggregation_keys.has_value())
    return base::unexpected(aggregation_keys.error());

  result.aggregation_keys_ = std::move(*aggregation_keys);

  result.debug_reporting_ =
      registration.FindBool("debug_reporting").value_or(false);

  result.reporting_origin_ = std::move(reporting_origin);
  return result;
}

// static
absl::optional<SourceRegistration> SourceRegistration::Create(
    uint64_t source_event_id,
    url::Origin destination,
    url::Origin reporting_origin,
    absl::optional<base::TimeDelta> expiry,
    absl::optional<base::TimeDelta> event_report_window,
    absl::optional<base::TimeDelta> aggregatable_report_window,
    int64_t priority,
    FilterData filter_data,
    absl::optional<uint64_t> debug_key,
    AggregationKeys aggregation_keys,
    bool debug_reporting) {
  if (!network::IsOriginPotentiallyTrustworthy(destination) ||
      !network::IsOriginPotentiallyTrustworthy(reporting_origin)) {
    return absl::nullopt;
  }

  SourceRegistration result;
  result.source_event_id_ = source_event_id;
  result.destination_ = std::move(destination);
  result.reporting_origin_ = std::move(reporting_origin);
  result.expiry_ = expiry;
  result.event_report_window_ = event_report_window;
  result.aggregatable_report_window_ = aggregatable_report_window;
  result.priority_ = priority;
  result.filter_data_ = std::move(filter_data);
  result.debug_key_ = debug_key;
  result.aggregation_keys_ = std::move(aggregation_keys);
  result.debug_reporting_ = debug_reporting;
  return result;
}

}  // namespace attribution_reporting
