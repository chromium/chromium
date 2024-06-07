// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

template <typename T>
void SerializeListIfNotEmpty(base::Value::Dict& dict,
                             std::string_view key,
                             const std::vector<T>& vec) {
  if (vec.empty()) {
    return;
  }

  base::Value::List list;
  for (const auto& value : vec) {
    list.Append(value.ToJson());
  }
  dict.Set(key, std::move(list));
}

template <typename T>
base::expected<std::vector<T>, TriggerRegistrationError> ParseList(
    base::Value* input_value,
    TriggerRegistrationError wrong_type,
    base::FunctionRef<base::expected<T, TriggerRegistrationError>(base::Value&)>
        build_element) {
  if (!input_value) {
    return {};
  }

  base::Value::List* list = input_value->GetIfList();
  if (!list) {
    return base::unexpected(wrong_type);
  }

  std::vector<T> vec;
  vec.reserve(list->size());

  for (auto& value : *list) {
    ASSIGN_OR_RETURN(T element, build_element(value));
    vec.emplace_back(std::move(element));
  }

  return vec;
}

}  // namespace

void RecordTriggerRegistrationError(TriggerRegistrationError error) {
  static_assert(TriggerRegistrationError::kMaxValue ==
                    TriggerRegistrationError::kEventValueInvalid,
                "Update ConversionTriggerRegistrationError enum.");
  base::UmaHistogramEnumeration("Conversions.TriggerRegistrationError11",
                                error);
}

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::Value::Dict dict) {
  TriggerRegistration registration;

  ASSIGN_OR_RETURN(registration.filters, FilterPair::FromJSON(dict));

  ASSIGN_OR_RETURN(registration.aggregatable_dedup_keys,
                   ParseList<AggregatableDedupKey>(
                       dict.Find(kAggregatableDeduplicationKeys),
                       TriggerRegistrationError::kAggregatableDedupKeyWrongType,
                       &AggregatableDedupKey::FromJSON));

  ASSIGN_OR_RETURN(registration.event_triggers,
                   ParseList<EventTriggerData>(
                       dict.Find(kEventTriggerData),
                       TriggerRegistrationError::kEventTriggerDataWrongType,
                       &EventTriggerData::FromJSON));

  ASSIGN_OR_RETURN(
      registration.aggregatable_trigger_data,
      ParseList<AggregatableTriggerData>(
          dict.Find(kAggregatableTriggerData),
          TriggerRegistrationError::kAggregatableTriggerDataWrongType,
          &AggregatableTriggerData::FromJSON));

  ASSIGN_OR_RETURN(
      registration.aggregatable_values,
      AggregatableValues::FromJSON(dict.Find(kAggregatableValues)));

  ASSIGN_OR_RETURN(
      registration.aggregation_coordinator_origin,
      ParseAggregationCoordinator(dict).transform_error([](ParseError) {
        return TriggerRegistrationError::kAggregationCoordinatorValueInvalid;
      }));

  registration.debug_key = ParseDebugKey(dict);
  registration.debug_reporting = ParseDebugReporting(dict);

  ASSIGN_OR_RETURN(registration.aggregatable_trigger_config,
                   AggregatableTriggerConfig::Parse(dict));

  // Deliberately ignoring errors for now to avoid dropping the registration
  // from the optional debug reporting feature.
  if (auto aggregatable_debug_reporting_config =
          AggregatableDebugReportingConfig::Parse(dict);
      aggregatable_debug_reporting_config.has_value()) {
    registration.aggregatable_debug_reporting_config =
        *std::move(aggregatable_debug_reporting_config);
  }

  return registration;
}

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(std::string_view json) {
  base::expected<TriggerRegistration, TriggerRegistrationError> trigger =
      base::unexpected(TriggerRegistrationError::kInvalidJson);

  std::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);

  if (value) {
    if (base::Value::Dict* dict = value->GetIfDict()) {
      trigger = Parse(std::move(*dict));
    } else {
      trigger = base::unexpected(TriggerRegistrationError::kRootWrongType);
    }
  }

  if (!trigger.has_value()) {
    RecordTriggerRegistrationError(trigger.error());
  }

  return trigger;
}

TriggerRegistration::TriggerRegistration() = default;

TriggerRegistration::~TriggerRegistration() = default;

TriggerRegistration::TriggerRegistration(const TriggerRegistration&) = default;

TriggerRegistration& TriggerRegistration::operator=(
    const TriggerRegistration&) = default;

TriggerRegistration::TriggerRegistration(TriggerRegistration&&) = default;

TriggerRegistration& TriggerRegistration::operator=(TriggerRegistration&&) =
    default;

base::Value::Dict TriggerRegistration::ToJson() const {
  base::Value::Dict dict;

  filters.SerializeIfNotEmpty(dict);

  SerializeListIfNotEmpty(dict, kAggregatableDeduplicationKeys,
                          aggregatable_dedup_keys);
  SerializeListIfNotEmpty(dict, kEventTriggerData, event_triggers);
  SerializeListIfNotEmpty(dict, kAggregatableTriggerData,
                          aggregatable_trigger_data);

  SerializeListIfNotEmpty(dict, kAggregatableValues, aggregatable_values);

  SerializeDebugKey(dict, debug_key);

  SerializeDebugReporting(dict, debug_reporting);

  if (aggregation_coordinator_origin.has_value()) {
    dict.Set(kAggregationCoordinatorOrigin,
             aggregation_coordinator_origin->Serialize());
  }

  aggregatable_trigger_config.Serialize(dict);

  aggregatable_debug_reporting_config.Serialize(dict);

  return dict;
}

}  // namespace attribution_reporting
