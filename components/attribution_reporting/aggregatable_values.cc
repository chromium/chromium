// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_values.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

bool FilteringIdEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAttributionReportingAggregatableFilteringIds);
}

bool IsValid(const AggregatableValues::Values& values) {
  return base::ranges::all_of(values, [](const auto& value) {
    return IsAggregatableValueInRange(value.second.value());
  });
}

base::expected<AggregatableValues::Values, TriggerRegistrationError>
ParseValues(const base::Value::Dict& dict,
            TriggerRegistrationError key_error,
            TriggerRegistrationError value_error) {
  AggregatableValues::Values::container_type container;

  for (auto [id, key_value] : dict) {
    ASSIGN_OR_RETURN(AggregatableValuesValue value,
                     AggregatableValuesValue::FromJSON(key_value, value_error));
    container.emplace_back(id, std::move(value));
  }
  return AggregatableValues::Values(base::sorted_unique, std::move(container));
}

}  // namespace

// static
std::optional<AggregatableValuesValue>
AggregatableValuesValue::AggregatableValuesValue::Create(
    int value,
    uint64_t filtering_id) {
  if (!IsAggregatableValueInRange(value)) {
    return std::nullopt;
  }
  return AggregatableValuesValue(value, filtering_id);
}

// static
base::expected<AggregatableValuesValue, TriggerRegistrationError>
AggregatableValuesValue::FromJSON(const base::Value& json,
                                  TriggerRegistrationError value_error) {
  int value;
  std::optional<uint64_t> filtering_id;

  if (const base::Value::Dict* dict = json.GetIfDict();
      dict && FilteringIdEnabled()) {
    const base::Value* value_v = dict->Find(kValue);
    if (!value_v) {
      return base::unexpected(value_error);
    }
    ASSIGN_OR_RETURN(value, ParseInt(*value_v),
                     [value_error](ParseError) { return value_error; });

    ASSIGN_OR_RETURN(filtering_id, ParseUint64(*dict, kFilteringId),
                     [value_error](ParseError) { return value_error; });
  } else {
    ASSIGN_OR_RETURN(value, ParseInt(json),
                     [value_error](ParseError) { return value_error; });
  }

  auto result = AggregatableValuesValue::Create(
      value, filtering_id.value_or(kDefaultFilteringId));
  if (!result) {
    return base::unexpected(value_error);
  }

  return *std::move(result);
}

AggregatableValuesValue::AggregatableValuesValue(uint32_t value,
                                                 uint64_t filtering_id)
    : value_(value), filtering_id_(filtering_id) {}

// static
std::optional<AggregatableValues> AggregatableValues::Create(
    Values values,
    FilterPair filters) {
  if (!IsValid(values)) {
    return std::nullopt;
  }

  return AggregatableValues(std::move(values), std::move(filters));
}

// static
base::expected<std::vector<AggregatableValues>, TriggerRegistrationError>
AggregatableValues::FromJSON(base::Value* input_value) {
  std::vector<AggregatableValues> configs;
  if (!input_value) {
    return configs;
  }

  if (base::Value::Dict* dict = input_value->GetIfDict()) {
    ASSIGN_OR_RETURN(
        Values values,
        ParseValues(*dict,
                    TriggerRegistrationError::kAggregatableValuesKeyTooLong,
                    TriggerRegistrationError::kAggregatableValuesValueInvalid));
    if (!values.empty()) {
      configs.push_back(AggregatableValues(std::move(values), FilterPair()));
    }
  } else if (base::Value::List* list = input_value->GetIfList()) {
    configs.reserve(list->size());
    for (auto& maybe_dict_value : *list) {
      base::Value::Dict* dict_value = maybe_dict_value.GetIfDict();
      if (!dict_value) {
        return base::unexpected(
            TriggerRegistrationError::kAggregatableValuesWrongType);
      }

      const base::Value::Dict* agg_values_dict = dict_value->FindDict(kValues);
      if (!agg_values_dict) {
        return base::unexpected(TriggerRegistrationError::
                                    kAggregatableValuesListValuesFieldMissing);
      }

      ASSIGN_OR_RETURN(
          Values values,
          ParseValues(
              *agg_values_dict,
              TriggerRegistrationError::kAggregatableValuesListKeyTooLong,
              TriggerRegistrationError::kAggregatableValuesListValueInvalid));
      ASSIGN_OR_RETURN(FilterPair filters, FilterPair::FromJSON(*dict_value));

      configs.push_back(
          AggregatableValues(std::move(values), std::move(filters)));
    }
  } else {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableValuesWrongType);
  }
  return configs;
}

base::Value::Dict AggregatableValuesValue::ToJson() const {
  CHECK(base::IsValueInRangeForNumericType<int>(value_));

  base::Value::Dict dict;

  dict.Set(kValue, static_cast<int>(value_));
  SerializeUint64(dict, kFilteringId, filtering_id_);

  return dict;
}

AggregatableValues::AggregatableValues() = default;

AggregatableValues::AggregatableValues(Values values, FilterPair filters)
    : values_(std::move(values)), filters_(std::move(filters)) {
  CHECK(IsValid(values_));
}

AggregatableValues::~AggregatableValues() = default;

AggregatableValues::AggregatableValues(const AggregatableValues&) = default;

AggregatableValues& AggregatableValues::operator=(const AggregatableValues&) =
    default;

AggregatableValues::AggregatableValues(AggregatableValues&&) = default;

AggregatableValues& AggregatableValues::operator=(AggregatableValues&&) =
    default;

base::Value::Dict AggregatableValues::ToJson() const {
  base::Value::Dict values_dict;
  for (const auto& [key, value] : values_) {
    if (FilteringIdEnabled()) {
      values_dict.Set(key, value.ToJson());
    } else {
      CHECK(base::IsValueInRangeForNumericType<int>(value.value()));
      values_dict.Set(key, static_cast<int>(value.value()));
    }
  }

  base::Value::Dict dict;
  dict.Set(kValues, std::move(values_dict));
  filters_.SerializeIfNotEmpty(dict);
  return dict;
}

}  // namespace attribution_reporting
