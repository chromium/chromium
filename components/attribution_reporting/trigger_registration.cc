// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/aggregation_service/features.h"
#include "components/aggregation_service/parsing_utils.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kAggregationCoordinatorOrigin[] =
    "aggregation_coordinator_origin";
constexpr char kAggregatableDeduplicationKeys[] =
    "aggregatable_deduplication_keys";
constexpr char kAggregatableTriggerData[] = "aggregatable_trigger_data";
constexpr char kAggregatableValues[] = "aggregatable_values";
constexpr char kEventTriggerData[] = "event_trigger_data";

base::expected<std::optional<SuitableOrigin>, TriggerRegistrationError>
ParseAggregationCoordinator(const base::Value* value) {
  // The default value is used for backward compatibility prior to this
  // attribute being added, but ideally this would invalidate the registration
  // if other aggregatable fields were present.
  if (!value) {
    return std::nullopt;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(
        TriggerRegistrationError::kAggregationCoordinatorWrongType);
  }

  std::optional<url::Origin> aggregation_coordinator =
      aggregation_service::ParseAggregationCoordinator(*str);
  if (!aggregation_coordinator.has_value()) {
    return base::unexpected(
        TriggerRegistrationError::kAggregationCoordinatorUnknownValue);
  }
  auto aggregation_coordinator_origin =
      SuitableOrigin::Create(*aggregation_coordinator);
  DCHECK(aggregation_coordinator_origin.has_value());
  return *aggregation_coordinator_origin;
}

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
  base::UmaHistogramEnumeration("Conversions.TriggerRegistrationError9", error);
}

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::Value::Dict dict) {
  TriggerRegistration registration;

  ASSIGN_OR_RETURN(registration.filters, FilterPair::FromJSON(dict));

  ASSIGN_OR_RETURN(
      registration.aggregatable_dedup_keys,
      ParseList<AggregatableDedupKey>(
          dict.Find(kAggregatableDeduplicationKeys),
          TriggerRegistrationError::kAggregatableDedupKeyListWrongType,
          &AggregatableDedupKey::FromJSON));

  ASSIGN_OR_RETURN(registration.event_triggers,
                   ParseList<EventTriggerData>(
                       dict.Find(kEventTriggerData),
                       TriggerRegistrationError::kEventTriggerDataListWrongType,
                       &EventTriggerData::FromJSON));

  ASSIGN_OR_RETURN(
      registration.aggregatable_trigger_data,
      ParseList<AggregatableTriggerData>(
          dict.Find(kAggregatableTriggerData),
          TriggerRegistrationError::kAggregatableTriggerDataListWrongType,
          &AggregatableTriggerData::FromJSON));

  ASSIGN_OR_RETURN(
      registration.aggregatable_values,
      AggregatableValues::FromJSON(dict.Find(kAggregatableValues)));

  if (base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders)) {
    ASSIGN_OR_RETURN(
        registration.aggregation_coordinator_origin,
        ParseAggregationCoordinator(dict.Find(kAggregationCoordinatorOrigin)));
  }

  registration.debug_key = ParseDebugKey(dict);
  registration.debug_reporting = ParseDebugReporting(dict);

  ASSIGN_OR_RETURN(registration.aggregatable_trigger_config,
                   AggregatableTriggerConfig::Parse(dict));

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
    if (value->is_dict()) {
      trigger = Parse(std::move(*value).TakeDict());
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

  if (base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders) &&
      aggregation_coordinator_origin.has_value()) {
    dict.Set(kAggregationCoordinatorOrigin,
             aggregation_coordinator_origin->Serialize());
  }

  aggregatable_trigger_config.Serialize(dict);

  return dict;
}

}  // namespace attribution_reporting
