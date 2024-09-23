// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;

base::TimeDelta AdjustExpiry(base::TimeDelta expiry, SourceType source_type) {
  switch (source_type) {
    case SourceType::kNavigation:
      return expiry;
    case SourceType::kEvent:
      return expiry.RoundToMultiple(base::Days(1));
  }
}

}  // namespace

void RecordSourceRegistrationError(SourceRegistrationError error) {
  base::UmaHistogramEnumeration("Conversions.SourceRegistrationError13", error);
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

namespace {

base::expected<SourceRegistration, SourceRegistrationError> ParseDict(
    base::Value::Dict registration,
    SourceType source_type) {
  ASSIGN_OR_RETURN(DestinationSet destination_set,
                   DestinationSet::FromJSON(registration.Find(kDestination)));
  SourceRegistration result(std::move(destination_set));

  ASSIGN_OR_RETURN(result.source_event_id,
                   ParseUint64(registration, kSourceEventId)
                       .transform(&ValueOrZero<uint64_t>),
                   [](ParseError) {
                     return SourceRegistrationError::kSourceEventIdValueInvalid;
                   });

  ASSIGN_OR_RETURN(result.priority, ParsePriority(registration),
                   [](ParseError) {
                     return SourceRegistrationError::kPriorityValueInvalid;
                   });

  if (const base::Value* value = registration.Find(kExpiry)) {
    ASSIGN_OR_RETURN(result.expiry,
                     ParseLegacyDuration(*value,
                                         /*clamp_min=*/kMinSourceExpiry,
                                         /*clamp_max=*/kMaxSourceExpiry),
                     [](ParseError) {
                       return SourceRegistrationError::kExpiryValueInvalid;
                     });

    result.expiry = AdjustExpiry(result.expiry, source_type);
  }

  if (const base::Value* value = registration.Find(kAggregatableReportWindow)) {
    ASSIGN_OR_RETURN(
        result.aggregatable_report_window,
        ParseLegacyDuration(*value,
                            /*clamp_min=*/kMinReportWindow,
                            /*clamp_max=*/result.expiry),
        [](ParseError) {
          return SourceRegistrationError::kAggregatableReportWindowValueInvalid;
        });
  } else {
    result.aggregatable_report_window = result.expiry;
  }

  ASSIGN_OR_RETURN(result.trigger_data_matching,
                   ParseTriggerDataMatching(registration));

  ASSIGN_OR_RETURN(result.event_level_epsilon,
                   EventLevelEpsilon::Parse(registration));

  ASSIGN_OR_RETURN(
      auto default_event_report_windows,
      EventReportWindows::FromJSON(registration, result.expiry, source_type));

  ASSIGN_OR_RETURN(
      result.trigger_specs,
      TriggerSpecs::ParseTopLevelTriggerData(
          registration, source_type, std::move(default_event_report_windows),
          result.trigger_data_matching));

  ASSIGN_OR_RETURN(result.filter_data,
                   FilterData::FromJSON(registration.Find(kFilterData)));

  ASSIGN_OR_RETURN(
      result.aggregation_keys,
      AggregationKeys::FromJSON(registration.Find(kAggregationKeys)));

  if (base::Value* scopes_value = registration.Find(kAttributionScopes);
      scopes_value &&
      base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    ASSIGN_OR_RETURN(result.attribution_scopes_data,
                     AttributionScopesData::FromJSON(*scopes_value));
  }

  result.debug_key = ParseDebugKey(registration);

  result.debug_reporting = ParseDebugReporting(registration);

  // Deliberately ignoring errors for now to avoid dropping the registration
  // from the optional debug reporting feature.
  if (auto aggregatable_debug_reporting_config =
          SourceAggregatableDebugReportingConfig::Parse(registration);
      aggregatable_debug_reporting_config.has_value()) {
    result.aggregatable_debug_reporting_config =
        *std::move(aggregatable_debug_reporting_config);
  }

  if (base::FeatureList::IsEnabled(attribution_reporting::features::
                                       kAttributionSourceDestinationLimit)) {
    ASSIGN_OR_RETURN(
        result.destination_limit_priority,
        ParseInt64(registration, kDestinationLimitPriority)
            .transform(&ValueOrZero<int64_t>),
        [](ParseError) {
          return SourceRegistrationError::kDestinationLimitPriorityInvalid;
        });
  }

  CHECK(result.IsValid());
  CHECK(result.IsValidForSourceType(source_type));

  base::UmaHistogramExactLinear(
      "Conversions.ScopesPerSourceRegistration",
      result.attribution_scopes_data.has_value()
          ? result.attribution_scopes_data->attribution_scopes_set()
                .scopes()
                .size()
          : 0,
      /*exclusive_max=*/attribution_reporting::kMaxScopesPerSource + 1);

  return result;
}

}  // namespace

// static
base::expected<SourceRegistration, SourceRegistrationError>
SourceRegistration::Parse(base::Value value, SourceType source_type) {
  if (base::Value::Dict* dict = value.GetIfDict()) {
    return ParseDict(std::move(*dict), source_type);
  } else {
    return base::unexpected(SourceRegistrationError::kRootWrongType);
  }
}

// static
base::expected<SourceRegistration, SourceRegistrationError>
SourceRegistration::Parse(std::string_view json, SourceType source_type) {
  base::expected<SourceRegistration, SourceRegistrationError> source =
      base::unexpected(SourceRegistrationError::kInvalidJson);

  if (std::optional<base::Value> value =
          base::JSONReader::Read(json, base::JSON_PARSE_RFC)) {
    source = Parse(*std::move(value), source_type);
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

  trigger_specs.Serialize(dict);

  SerializeTimeDeltaInSeconds(dict, kAggregatableReportWindow,
                              aggregatable_report_window);

  SerializeDebugKey(dict, debug_key);
  SerializeDebugReporting(dict, debug_reporting);

  Serialize(dict, trigger_data_matching);

  event_level_epsilon.Serialize(dict);

  aggregatable_debug_reporting_config.Serialize(dict);

  if (attribution_scopes_data.has_value() &&
      base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    dict.Set(kAttributionScopes, attribution_scopes_data->ToJson());
  }

  if (base::FeatureList::IsEnabled(attribution_reporting::features::
                                       kAttributionSourceDestinationLimit)) {
    SerializeInt64(dict, kDestinationLimitPriority, destination_limit_priority);
  }

  return dict;
}

bool SourceRegistration::IsValid() const {
  if (expiry < kMinSourceExpiry || expiry > kMaxSourceExpiry) {
    return false;
  }

  for (const auto& spec : trigger_specs.specs()) {
    if (!spec.event_report_windows().IsValidForExpiry(expiry)) {
      return false;
    }
  }

  if (aggregatable_report_window < kMinReportWindow ||
      aggregatable_report_window > expiry) {
    return false;
  }

  return true;
}

bool SourceRegistration::IsValidForSourceType(SourceType source_type) const {
  return expiry == AdjustExpiry(expiry, source_type);
}

}  // namespace attribution_reporting
