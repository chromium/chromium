// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregation_keys.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

bool IsValid(const AggregationKeys::Keys& keys) {
  return keys.size() <= kMaxAggregationKeysPerSourceOrTrigger &&
         base::ranges::all_of(keys, [](const auto& key) {
           return key.first.size() <= kMaxBytesPerAggregationKeyId;
         });
}

}  // namespace

// static
absl::optional<AggregationKeys> AggregationKeys::FromKeys(Keys keys) {
  if (!IsValid(keys))
    return absl::nullopt;

  return AggregationKeys(std::move(keys));
}

// static
base::expected<AggregationKeys, SourceRegistrationError>
AggregationKeys::FromJSON(const base::Value* value) {
  // TODO(johnidel): Consider logging registration JSON metrics here.
  if (!value)
    return AggregationKeys();

  const base::Value::Dict* dict = value->GetIfDict();
  if (!dict)
    return base::unexpected(SourceRegistrationError::kAggregationKeysWrongType);

  const size_t num_keys = dict->size();

  if (num_keys > kMaxAggregationKeysPerSourceOrTrigger) {
    return base::unexpected(
        SourceRegistrationError::kAggregationKeysTooManyKeys);
  }

  Keys::container_type keys;
  keys.reserve(num_keys);

  for (auto [key_id, maybe_string_value] : *dict) {
    if (key_id.size() > kMaxBytesPerAggregationKeyId) {
      return base::unexpected(
          SourceRegistrationError::kAggregationKeysKeyTooLong);
    }

    const std::string* s = maybe_string_value.GetIfString();
    if (!s) {
      return base::unexpected(
          SourceRegistrationError::kAggregationKeysValueWrongType);
    }

    absl::uint128 key;

    if (!base::StartsWith(*s, "0x", base::CompareCase::INSENSITIVE_ASCII) ||
        !base::HexStringToUInt128(*s, &key)) {
      return base::unexpected(
          SourceRegistrationError::kAggregationKeysValueWrongFormat);
    }

    keys.emplace_back(key_id, key);
  }

  return AggregationKeys(Keys(base::sorted_unique, std::move(keys)));
}

AggregationKeys::AggregationKeys(Keys keys) : keys_(std::move(keys)) {
  DCHECK(IsValid(keys_));
}

AggregationKeys::AggregationKeys() = default;

AggregationKeys::~AggregationKeys() = default;

AggregationKeys::AggregationKeys(const AggregationKeys&) = default;

AggregationKeys::AggregationKeys(AggregationKeys&&) = default;

AggregationKeys& AggregationKeys::operator=(const AggregationKeys&) = default;

AggregationKeys& AggregationKeys::operator=(AggregationKeys&&) = default;

}  // namespace attribution_reporting
