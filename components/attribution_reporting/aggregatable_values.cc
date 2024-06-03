// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_values.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

bool IsValid(const AggregatableValues::Values& values) {
  return base::ranges::all_of(values, [](const auto& value) {
    return AggregationKeyIdHasValidLength(value.first) &&
           IsAggregatableValueInRange(value.second);
  });
}

base::expected<AggregatableValues::Values, mojom::TriggerRegistrationError>
ParseValues(const base::Value::Dict& dict,
            TriggerRegistrationError key_error,
            TriggerRegistrationError value_error) {
  AggregatableValues::Values::container_type container;

  for (auto [id, key_value] : dict) {
    if (!AggregationKeyIdHasValidLength(id)) {
      return base::unexpected(key_error);
    }

    std::optional<int> int_value = key_value.GetIfInt();
    if (!int_value.has_value() || !IsAggregatableValueInRange(*int_value)) {
      return base::unexpected(value_error);
    }

    container.emplace_back(id, *int_value);
  }
  return AggregatableValues::Values(base::sorted_unique, std::move(container));
}

}  // namespace

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
base::expected<std::vector<AggregatableValues>, mojom::TriggerRegistrationError>
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

AggregatableValues::AggregatableValues() = default;

AggregatableValues::AggregatableValues(Values values, FilterPair filters)
    : values_(std::move(values)), filters_(std::move(filters)) {
  CHECK(IsValid(values_), base::NotFatalUntil::M128);
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
    CHECK(base::IsValueInRangeForNumericType<int>(value),
          base::NotFatalUntil::M128);
    values_dict.Set(key, static_cast<int>(value));
  }

  base::Value::Dict dict;
  dict.Set(kValues, std::move(values_dict));
  filters_.SerializeIfNotEmpty(dict);
  return dict;
}

}  // namespace attribution_reporting
