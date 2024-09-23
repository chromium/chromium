// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregation_keys.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

bool AggregationKeyIdHasValidLength(const std::string& key) {
  return key.size() <= AggregationKeys::kMaxBytesPerAggregationKeyId;
}

bool IsValid(const AggregationKeys::Keys& keys) {
  return keys.size() <= kMaxAggregationKeysPerSource &&
         base::ranges::all_of(keys, [](const auto& key) {
           return AggregationKeyIdHasValidLength(key.first);
         });
}

void RecordAggregatableKeysPerSource(base::HistogramBase::Sample count) {
  const int kExclusiveMaxHistogramValue = 101;

  static_assert(
      kMaxAggregationKeysPerSource < kExclusiveMaxHistogramValue,
      "Bump the version for histogram Conversions.AggregatableKeysPerSource");

  base::UmaHistogramCounts100("Conversions.AggregatableKeysPerSource", count);
}

}  // namespace

// static
std::optional<AggregationKeys> AggregationKeys::FromKeys(Keys keys) {
  if (!IsValid(keys))
    return std::nullopt;

  return AggregationKeys(std::move(keys));
}

// static
base::expected<AggregationKeys, SourceRegistrationError>
AggregationKeys::FromJSON(const base::Value* value) {
  if (!value)
    return AggregationKeys();

  const base::Value::Dict* dict = value->GetIfDict();
  if (!dict)
    return base::unexpected(
        SourceRegistrationError::kAggregationKeysDictInvalid);

  const size_t num_keys = dict->size();

  if (num_keys > kMaxAggregationKeysPerSource) {
    return base::unexpected(
        SourceRegistrationError::kAggregationKeysDictInvalid);
  }

  RecordAggregatableKeysPerSource(num_keys);

  Keys::container_type keys;
  keys.reserve(num_keys);

  for (auto [key_id, maybe_string_value] : *dict) {
    if (!AggregationKeyIdHasValidLength(key_id)) {
      return base::unexpected(
          SourceRegistrationError::kAggregationKeysKeyTooLong);
    }

    ASSIGN_OR_RETURN(
        absl::uint128 key, ParseAggregationKeyPiece(maybe_string_value),
        [](ParseError) {
          return SourceRegistrationError::kAggregationKeysValueInvalid;
        });

    keys.emplace_back(key_id, key);
  }

  return AggregationKeys(Keys(base::sorted_unique, std::move(keys)));
}

AggregationKeys::AggregationKeys(Keys keys) : keys_(std::move(keys)) {
  CHECK(IsValid(keys_));
}

AggregationKeys::AggregationKeys() = default;

AggregationKeys::~AggregationKeys() = default;

AggregationKeys::AggregationKeys(const AggregationKeys&) = default;

AggregationKeys::AggregationKeys(AggregationKeys&&) = default;

AggregationKeys& AggregationKeys::operator=(const AggregationKeys&) = default;

AggregationKeys& AggregationKeys::operator=(AggregationKeys&&) = default;

base::Value::Dict AggregationKeys::ToJson() const {
  base::Value::Dict dict;
  for (const auto& [key, value] : keys_) {
    dict.Set(key, HexEncodeAggregationKey(value));
  }
  return dict;
}

}  // namespace attribution_reporting
