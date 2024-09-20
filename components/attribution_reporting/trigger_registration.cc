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
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
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

  auto list = base::Value::List::with_capacity(vec.size());
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

bool ContributionsFilteringIdsFitWithinMaxBytes(
    const std::vector<AggregatableValues>& aggregatable_values,
    AggregatableFilteringIdsMaxBytes max_bytes) {
  for (const AggregatableValues& values : aggregatable_values) {
    for (const std::pair<std::string, AggregatableValuesValue>& value :
         values.values()) {
      if (!max_bytes.CanEncompass(value.second.filtering_id())) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace

void RecordTriggerRegistrationError(TriggerRegistrationError error) {
  base::UmaHistogramEnumeration("Conversions.TriggerRegistrationError11",
                                error);
}

namespace {

base::expected<TriggerRegistration, TriggerRegistrationError> ParseDict(
    base::Value::Dict dict) {
  TriggerRegistration registration;

  ASSIGN_OR_RETURN(
      registration.aggregation_coordinator_origin,
      ParseAggregationCoordinator(dict).transform_error([](ParseError) {
        return TriggerRegistrationError::kAggregationCoordinatorValueInvalid;
      }));

  ASSIGN_OR_RETURN(registration.aggregatable_trigger_config,
                   AggregatableTriggerConfig::Parse(dict));

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

  if (base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    ASSIGN_OR_RETURN(registration.attribution_scopes,
                     AttributionScopesSet::FromJSON(dict));
  }

  registration.debug_key = ParseDebugKey(dict);
  registration.debug_reporting = ParseDebugReporting(dict);

  // Deliberately ignoring errors for now to avoid dropping the registration
  // from the optional debug reporting feature.
  if (auto aggregatable_debug_reporting_config =
          AggregatableDebugReportingConfig::Parse(dict);
      aggregatable_debug_reporting_config.has_value()) {
    registration.aggregatable_debug_reporting_config =
        *std::move(aggregatable_debug_reporting_config);
  }

  if (!ContributionsFilteringIdsFitWithinMaxBytes(
          registration.aggregatable_values,
          registration.aggregatable_trigger_config
              .aggregatable_filtering_id_max_bytes())) {
    return base::unexpected(
        dict.FindList(kAggregatableValues)
            ? TriggerRegistrationError::kAggregatableValuesListValueInvalid
            : TriggerRegistrationError::kAggregatableValuesValueInvalid);
  }

  base::UmaHistogramCounts100("Conversions.ScopesPerTriggerRegistration",
                              registration.attribution_scopes.scopes().size());

  return registration;
}

}  // namespace

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(base::Value value) {
  if (base::Value::Dict* dict = value.GetIfDict()) {
    return ParseDict(std::move(*dict));
  } else {
    return base::unexpected(TriggerRegistrationError::kRootWrongType);
  }
}

// static
base::expected<TriggerRegistration, TriggerRegistrationError>
TriggerRegistration::Parse(std::string_view json) {
  base::expected<TriggerRegistration, TriggerRegistrationError> trigger =
      base::unexpected(TriggerRegistrationError::kInvalidJson);

  if (std::optional<base::Value> value =
          base::JSONReader::Read(json, base::JSON_PARSE_RFC)) {
    trigger = Parse(*std::move(value));
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

  if (base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    attribution_scopes.SerializeForTrigger(dict);
  }
  return dict;
}

bool TriggerRegistration::IsValid() const {
  return ContributionsFilteringIdsFitWithinMaxBytes(
      aggregatable_values,
      aggregatable_trigger_config.aggregatable_filtering_id_max_bytes());
}

}  // namespace attribution_reporting
