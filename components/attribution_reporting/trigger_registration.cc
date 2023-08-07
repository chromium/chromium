// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/aggregation_service/features.h"
#include "components/aggregation_service/parsing_utils.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
constexpr char kAggregatableSourceRegistrationTime[] =
    "aggregatable_source_registration_time";

constexpr char kInclude[] = "include";
constexpr char kExclude[] = "exclude";

base::expected<absl::optional<SuitableOrigin>, TriggerRegistrationError>
ParseAggregationCoordinator(const base::Value* value) {
  // The default value is used for backward compatibility prior to this
  // attribute being added, but ideally this would invalidate the registration
  // if other aggregatable fields were present.
  if (!value) {
    return absl::nullopt;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(
        TriggerRegistrationError::kAggregationCoordinatorWrongType);
  }

  absl::optional<url::Origin> aggregation_coordinator =
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
    vec.push_back(std::move(element));
  }

  return vec;
}

base::expected<mojom::SourceRegistrationTimeConfig, TriggerRegistrationError>
ParseAggregatableSourceRegistrationTime(const base::Value* value) {
  if (!value) {
    return mojom::SourceRegistrationTimeConfig::kExclude;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableSourceRegistrationTimeWrongType);
  }

  if (*str == kInclude) {
    return mojom::SourceRegistrationTimeConfig::kInclude;
  }

  if (*str == kExclude) {
    return mojom::SourceRegistrationTimeConfig::kExclude;
  }

  return base::unexpected(TriggerRegistrationError::
                              kAggregatableSourceRegistrationTimeUnknownValue);
}

std::string SerializeAggregatableSourceRegistrationTime(
    mojom::SourceRegistrationTimeConfig config) {
  switch (config) {
    case mojom::SourceRegistrationTimeConfig::kInclude:
      return kInclude;
    case mojom::SourceRegistrationTimeConfig::kExclude:
      return kExclude;
  }
}

}  // namespace

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::Value::Dict registration) {
  ASSIGN_OR_RETURN(FilterPair filters, FilterPair::FromJSON(registration));
  ASSIGN_OR_RETURN(
      std::vector<AggregatableDedupKey> aggregatable_dedup_keys,
      ParseList<AggregatableDedupKey>(
          registration.Find(kAggregatableDeduplicationKeys),
          TriggerRegistrationError::kAggregatableDedupKeyListWrongType,
          &AggregatableDedupKey::FromJSON));
  ASSIGN_OR_RETURN(std::vector<EventTriggerData> event_triggers,
                   ParseList<EventTriggerData>(
                       registration.Find(kEventTriggerData),
                       TriggerRegistrationError::kEventTriggerDataListWrongType,
                       &EventTriggerData::FromJSON));
  ASSIGN_OR_RETURN(
      std::vector<AggregatableTriggerData> aggregatable_trigger_data,
      ParseList<AggregatableTriggerData>(
          registration.Find(kAggregatableTriggerData),
          TriggerRegistrationError::kAggregatableTriggerDataListWrongType,
          &AggregatableTriggerData::FromJSON));
  ASSIGN_OR_RETURN(
      AggregatableValues aggregatable_values,
      AggregatableValues::FromJSON(registration.Find(kAggregatableValues)));
  absl::optional<SuitableOrigin> aggregation_coordinator;
  if (base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders)) {
    ASSIGN_OR_RETURN(aggregation_coordinator,
                     ParseAggregationCoordinator(
                         registration.Find(kAggregationCoordinatorOrigin)));
  }
  absl::optional<uint64_t> debug_key = ParseDebugKey(registration);
  bool debug_reporting = ParseDebugReporting(registration);

  auto source_registration_time_config =
      mojom::SourceRegistrationTimeConfig::kInclude;
  if (base::FeatureList::IsEnabled(
          kAttributionReportingNullAggregatableReports)) {
    ASSIGN_OR_RETURN(source_registration_time_config,
                     ParseAggregatableSourceRegistrationTime(registration.Find(
                         kAggregatableSourceRegistrationTime)));
  }

  return TriggerRegistration(
      std::move(filters), debug_key, std::move(aggregatable_dedup_keys),
      std::move(event_triggers), std::move(aggregatable_trigger_data),
      std::move(aggregatable_values), debug_reporting, aggregation_coordinator,
      source_registration_time_config);
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
    base::UmaHistogramEnumeration("Conversions.TriggerRegistrationError6",
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
    absl::optional<SuitableOrigin> aggregation_coordinator_origin,
    mojom::SourceRegistrationTimeConfig source_registration_time_config)
    : filters(std::move(filters)),
      debug_key(debug_key),
      aggregatable_dedup_keys(std::move(aggregatable_dedup_keys)),
      event_triggers(std::move(event_triggers)),
      aggregatable_trigger_data(aggregatable_trigger_data),
      aggregatable_values(std::move(aggregatable_values)),
      debug_reporting(debug_reporting),
      aggregation_coordinator_origin(std::move(aggregation_coordinator_origin)),
      source_registration_time_config(source_registration_time_config) {}

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

  if (base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders) &&
      aggregation_coordinator_origin.has_value()) {
    dict.Set(kAggregationCoordinatorOrigin,
             aggregation_coordinator_origin->Serialize());
  }

  if (base::FeatureList::IsEnabled(
          kAttributionReportingNullAggregatableReports)) {
    dict.Set(kAggregatableSourceRegistrationTime,
             SerializeAggregatableSourceRegistrationTime(
                 source_registration_time_config));
  }

  return dict;
}

}  // namespace attribution_reporting
