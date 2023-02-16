// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/aggregation_service/parsing_utils.h"
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

// Records the Conversions.AggregatableTriggerDataLength metric.
void RecordAggregatableTriggerDataPerTrigger(
    base::HistogramBase::Sample count) {
  const int kExclusiveMaxHistogramValue = 101;

  static_assert(
      kMaxAggregatableTriggerDataPerTrigger < kExclusiveMaxHistogramValue,
      "Bump the version for histogram "
      "Conversions.AggregatableTriggerDataLength");

  base::UmaHistogramCounts100("Conversions.AggregatableTriggerDataLength",
                              count);
}

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

}  // namespace

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::Value::Dict registration) {
  auto filters = FilterPair::FromJSON(registration);
  if (!filters.has_value())
    return base::unexpected(filters.error());

  auto aggregatable_dedup_keys =
      AggregatableDedupKeyList::Build<TriggerRegistrationError>(
          registration.Find(kAggregatableDeduplicationKeys),
          TriggerRegistrationError::kAggregatableDedupKeyListWrongType,
          TriggerRegistrationError::kAggregatableDedupKeyListTooLong,
          &AggregatableDedupKey::FromJSON);
  if (!aggregatable_dedup_keys.has_value()) {
    return base::unexpected(aggregatable_dedup_keys.error());
  }

  auto event_triggers = EventTriggerDataList::Build<TriggerRegistrationError>(
      registration.Find(kEventTriggerData),
      TriggerRegistrationError::kEventTriggerDataListWrongType,
      TriggerRegistrationError::kEventTriggerDataListTooLong,
      &EventTriggerData::FromJSON);
  if (!event_triggers.has_value())
    return base::unexpected(event_triggers.error());

  auto aggregatable_trigger_data =
      AggregatableTriggerDataList::Build<TriggerRegistrationError>(
          registration.Find(kAggregatableTriggerData),
          TriggerRegistrationError::kAggregatableTriggerDataListWrongType,
          TriggerRegistrationError::kAggregatableTriggerDataListTooLong,
          &AggregatableTriggerData::FromJSON);
  if (!aggregatable_trigger_data.has_value())
    return base::unexpected(aggregatable_trigger_data.error());

  RecordAggregatableTriggerDataPerTrigger(
      aggregatable_trigger_data->vec().size());

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
    base::UmaHistogramEnumeration("Conversions.TriggerRegistrationError2",
                                  trigger.error());
  }

  return trigger;
}

TriggerRegistration::TriggerRegistration() = default;

TriggerRegistration::TriggerRegistration(
    FilterPair filters,
    absl::optional<uint64_t> debug_key,
    AggregatableDedupKeyList aggregatable_dedup_keys,
    EventTriggerDataList event_triggers,
    AggregatableTriggerDataList aggregatable_trigger_data,
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
                          aggregatable_dedup_keys.vec());
  SerializeListIfNotEmpty(dict, kEventTriggerData, event_triggers.vec());
  SerializeListIfNotEmpty(dict, kAggregatableTriggerData,
                          aggregatable_trigger_data.vec());

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
