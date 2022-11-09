// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_values.h"

#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "components/attribution_reporting/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

bool IsValid(const AggregatableValues::Values& values) {
  if (values.size() > kMaxAggregationKeysPerSourceOrTrigger)
    return false;

  return base::ranges::all_of(values, [](const auto& value) {
    return value.first.size() <= kMaxBytesPerAggregationKeyId &&
           value.second > 0 && value.second <= kMaxAggregatableValue;
  });
}

}  // namespace

// static
absl::optional<AggregatableValues> AggregatableValues::Create(Values values) {
  if (!IsValid(values))
    return absl::nullopt;

  return AggregatableValues(std::move(values));
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

}  // namespace attribution_reporting
