// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_values.h"

#include <utility>

#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

bool IsValueInRange(int value) {
  return value > 0 && value <= kMaxAggregatableValue;
}

bool IsValid(const AggregatableValues::Values& values) {
  return base::ranges::all_of(values, [](const auto& value) {
    return AggregationKeyIdHasValidLength(value.first) &&
           IsValueInRange(value.second);
  });
}

}  // namespace

// static
absl::optional<AggregatableValues> AggregatableValues::Create(Values values) {
  if (!IsValid(values))
    return absl::nullopt;

  return AggregatableValues(std::move(values));
}

// static
base::expected<AggregatableValues, mojom::TriggerRegistrationError>
AggregatableValues::FromJSON(const base::Value* input_value) {
  if (!input_value)
    return AggregatableValues();

  const base::Value::Dict* dict = input_value->GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableValuesWrongType);
  }

  Values::container_type container;

  for (auto [id, key_value] : *dict) {
    if (!AggregationKeyIdHasValidLength(id)) {
      return base::unexpected(
          TriggerRegistrationError::kAggregatableValuesKeyTooLong);
    }

    absl::optional<int> int_value = key_value.GetIfInt();
    if (!int_value.has_value()) {
      return base::unexpected(
          TriggerRegistrationError::kAggregatableValuesValueWrongType);
    }

    if (!IsValueInRange(*int_value)) {
      return base::unexpected(
          TriggerRegistrationError::kAggregatableValuesValueOutOfRange);
    }

    container.emplace_back(id, *int_value);
  }

  return AggregatableValues(Values(base::sorted_unique, std::move(container)));
}

AggregatableValues::AggregatableValues() = default;

AggregatableValues::AggregatableValues(Values values)
    : values_(std::move(values)) {
  DCHECK(IsValid(values_));
}

AggregatableValues::~AggregatableValues() = default;

AggregatableValues::AggregatableValues(const AggregatableValues&) = default;

AggregatableValues& AggregatableValues::operator=(const AggregatableValues&) =
    default;

AggregatableValues::AggregatableValues(AggregatableValues&&) = default;

AggregatableValues& AggregatableValues::operator=(AggregatableValues&&) =
    default;

base::Value::Dict AggregatableValues::ToJson() const {
  base::Value::Dict dict;
  for (auto [key, value] : values_) {
    DCHECK(base::IsValueInRangeForNumericType<int>(value));
    dict.Set(key, static_cast<int>(value));
  }
  return dict;
}

}  // namespace attribution_reporting
