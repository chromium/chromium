// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_data.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "components/attribution_reporting/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

bool AreSourceKeysValid(const base::flat_set<std::string>& source_keys) {
  if (source_keys.size() > kMaxAggregationKeysPerSourceOrTrigger)
    return false;

  return base::ranges::all_of(source_keys, [](const auto& key) {
    return key.size() <= kMaxBytesPerAggregationKeyId;
  });
}

}  // namespace

// static
absl::optional<AggregatableTriggerData> AggregatableTriggerData::Create(
    absl::uint128 key_piece,
    base::flat_set<std::string> source_keys,
    Filters filters,
    Filters not_filters) {
  if (!AreSourceKeysValid(source_keys))
    return absl::nullopt;

  return AggregatableTriggerData(key_piece, std::move(source_keys),
                                 std::move(filters), std::move(not_filters));
}

AggregatableTriggerData::AggregatableTriggerData(
    absl::uint128 key_piece,
    base::flat_set<std::string> source_keys,
    Filters filters,
    Filters not_filters)
    : key_piece_(key_piece),
      source_keys_(std::move(source_keys)),
      filters_(std::move(filters)),
      not_filters_(std::move(not_filters)) {
  DCHECK(AreSourceKeysValid(source_keys_));
}

AggregatableTriggerData::~AggregatableTriggerData() = default;

AggregatableTriggerData::AggregatableTriggerData(
    const AggregatableTriggerData&) = default;

AggregatableTriggerData& AggregatableTriggerData::operator=(
    const AggregatableTriggerData&) = default;

AggregatableTriggerData::AggregatableTriggerData(AggregatableTriggerData&&) =
    default;

AggregatableTriggerData& AggregatableTriggerData::operator=(
    AggregatableTriggerData&&) = default;

}  // namespace attribution_reporting
