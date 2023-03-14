// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/aggregation_service/parsing_utils.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::aggregation_service::mojom::AggregationCoordinator;
using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kAggregationCoordinatorIdentifier[] =
    "aggregation_coordinator_identifier";
constexpr char kAggregatableDeduplicationKeys[] =
    "aggregatable_deduplication_keys";
constexpr char kAggregatableTriggerData[] = "aggregatable_trigger_data";
constexpr char kAggregatableValues[] = "aggregatable_values";
constexpr char kEventTriggerData[] = "event_trigger_data";

base::expected<AggregationCoordinator, TriggerRegistrationError>
ParseAggregationCoordinator(const base::Value* value) {
  // The default value is used for backward compatibility prior to this
  // attribute being added, but ideally this would invalidate the registration
  // if other aggregatable fields were present.
  if (!value)
    return AggregationCoordinator::kDefault;

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(
        TriggerRegistrationError::kAggregationCoordinatorWrongType);
  }

  absl::optional<AggregationCoordinator> aggregation_coordinator =
      aggregation_service::ParseAggregationCoordinator(*str);
  if (!aggregation_coordinator.has_value()) {
    return base::unexpected(
        TriggerRegistrationError::kAggregationCoordinatorUnknownValue);
  }

  return *aggregation_coordinator;
}

template <typename T>
void SerializeListIfNotEmpty(base::Value::Dict& dict,
                             base::StringPiece key,
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
  std::vector<T> vec;

  if (!input_value) {
    return vec;
  }

  base::Value::List* list = input_value->GetIfList();
  if (!list) {
    return base::unexpected(wrong_type);
  }

  vec.reserve(list->size());

  for (auto& value : *list) {
    base::expected<T, TriggerRegistrationError> element = build_element(value);
    if (!element.has_value()) {
      return base::unexpected(element.error());
    }

    vec.push_back(std::move(*element));
  }

  return vec;
}

}  // namespace

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::Value::Dict registration) {
  auto filters = FilterPair::FromJSON(registration);
  if (!filters.has_value())
    return base::unexpected(filters.error());

  auto aggregatable_dedup_keys = ParseList<AggregatableDedupKey>(
      registration.Find(kAggregatableDeduplicationKeys),
      TriggerRegistrationError::kAggregatableDedupKeyListWrongType,
      &AggregatableDedupKey::FromJSON);
  if (!aggregatable_dedup_keys.has_value()) {
    return base::unexpected(aggregatable_dedup_keys.error());
  }

  auto event_triggers = ParseList<EventTriggerData>(
      registration.Find(kEventTriggerData),
      TriggerRegistrationError::kEventTriggerDataListWrongType,
      &EventTriggerData::FromJSON);
  if (!event_triggers.has_value())
    return base::unexpected(event_triggers.error());

  auto aggregatable_trigger_data = ParseList<AggregatableTriggerData>(
      registration.Find(kAggregatableTriggerData),
      TriggerRegistrationError::kAggregatableTriggerDataListWrongType,
      &AggregatableTriggerData::FromJSON);
  if (!aggregatable_trigger_data.has_value())
    return base::unexpected(aggregatable_trigger_data.error());

  auto aggregatable_values =
      AggregatableValues::FromJSON(registration.Find(kAggregatableValues));
  if (!aggregatable_values.has_value())
    return base::unexpected(aggregatable_values.error());

  auto aggregation_coordinator = ParseAggregationCoordinator(
      registration.Find(kAggregationCoordinatorIdentifier));
  if (!aggregation_coordinator.has_value())
    return base::unexpected(aggregation_coordinator.error());

  absl::optional<uint64_t> debug_key = ParseDebugKey(registration);
  bool debug_reporting = ParseDebugReporting(registration);

  return TriggerRegistration(
      std::move(*filters), debug_key, std::move(*aggregatable_dedup_keys),
      std::move(*event_triggers), std::move(*aggregatable_trigger_data),
      std::move(*aggregatable_values), debug_reporting,
      *aggregation_coordinator);
}

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::StringPiece json) {
  base::expected<TriggerRegistration, TriggerRegistrationError> trigger =
      base::unexpected(TriggerRegistrationError::kInvalidJson);

  absl::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);

  if (value) {
    if (value->is_dict()) {
      trigger = Parse(std::move(*value).TakeDict());
    } else {
      trigger = base::unexpected(TriggerRegistrationError::kRootWrongType);
    }
  }

  if (!trigger.has_value()) {
    base::UmaHistogramEnumeration("Conversions.TriggerRegistrationError4",
                                  trigger.error());
  }

  return trigger;
}

TriggerRegistration::TriggerRegistration() = default;

TriggerRegistration::TriggerRegistration(
    FilterPair filters,
    absl::optional<uint64_t> debug_key,
    std::vector<AggregatableDedupKey> aggregatable_dedup_keys,
    std::vector<EventTriggerData> event_triggers,
    std::vector<AggregatableTriggerData> aggregatable_trigger_data,
    AggregatableValues aggregatable_values,
    bool debug_reporting,
    aggregation_service::mojom::AggregationCoordinator aggregation_coordinator)
    : filters(std::move(filters)),
      debug_key(debug_key),
      aggregatable_dedup_keys(std::move(aggregatable_dedup_keys)),
      event_triggers(std::move(event_triggers)),
      aggregatable_trigger_data(aggregatable_trigger_data),
      aggregatable_values(std::move(aggregatable_values)),
      debug_reporting(debug_reporting),
      aggregation_coordinator(aggregation_coordinator) {}

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

  if (!aggregatable_values.values().empty()) {
    dict.Set(kAggregatableValues, aggregatable_values.ToJson());
  }

  SerializeDebugKey(dict, debug_key);

  SerializeDebugReporting(dict, debug_reporting);

  dict.Set(kAggregationCoordinatorIdentifier,
           aggregation_service::SerializeAggregationCoordinator(
               aggregation_coordinator));

  return dict;
}

}  // namespace attribution_reporting
