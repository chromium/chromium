// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_data.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kKeyPiece[] = "key_piece";
constexpr char kSourceKeys[] = "source_keys";

bool AreSourceKeysValid(const AggregatableTriggerData::Keys& source_keys) {
  if (source_keys.size() > kMaxAggregationKeysPerSourceOrTrigger)
    return false;

  return base::ranges::all_of(source_keys, [](const auto& key) {
    return AggregationKeyIdHasValidLength(key);
  });
}

base::expected<absl::uint128, TriggerRegistrationError> ParseKeyPiece(
    const base::Value::Dict& registration) {
  const base::Value* v = registration.Find(kKeyPiece);
  if (!v) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataKeyPieceMissing);
  }

  const std::string* s = v->GetIfString();
  if (!s) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataKeyPieceWrongType);
  }

  absl::optional<absl::uint128> key_piece = StringToAggregationKeyPiece(*s);
  if (!key_piece) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataKeyPieceWrongFormat);
  }
  return *key_piece;
}

base::expected<AggregatableTriggerData::Keys, TriggerRegistrationError>
ParseSourceKeys(base::Value::Dict& registration) {
  base::Value* v = registration.Find(kSourceKeys);
  if (!v)
    return AggregatableTriggerData::Keys();

  base::Value::List* l = v->GetIfList();
  if (!l) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataSourceKeysWrongType);
  }

  const size_t num_source_keys = l->size();

  if (num_source_keys > kMaxAggregationKeysPerSourceOrTrigger) {
    return base::unexpected(TriggerRegistrationError::
                                kAggregatableTriggerDataSourceKeysTooManyKeys);
  }

  AggregatableTriggerData::Keys source_keys;
  source_keys.reserve(num_source_keys);

  for (auto& maybe_string_value : *l) {
    std::string* s = maybe_string_value.GetIfString();
    if (!s) {
      return base::unexpected(
          TriggerRegistrationError::
              kAggregatableTriggerDataSourceKeysKeyWrongType);
    }
    if (!AggregationKeyIdHasValidLength(*s)) {
      return base::unexpected(TriggerRegistrationError::
                                  kAggregatableTriggerDataSourceKeysKeyTooLong);
    }

    source_keys.push_back(std::move(*s));
  }

  return source_keys;
}

void SerializeSourceKeysIfNotEmpty(base::Value::Dict& dict,
                                   const AggregatableTriggerData::Keys& keys) {
  if (keys.empty())
    return;

  base::Value::List list;
  for (const std::string& key : keys) {
    list.Append(key);
  }
  dict.Set(kSourceKeys, std::move(list));
}

}  // namespace

// static
absl::optional<AggregatableTriggerData> AggregatableTriggerData::Create(
    absl::uint128 key_piece,
    Keys source_keys,
    FilterPair filters) {
  if (!AreSourceKeysValid(source_keys))
    return absl::nullopt;

  return AggregatableTriggerData(key_piece, std::move(source_keys),
                                 std::move(filters));
}

// static
base::expected<AggregatableTriggerData, TriggerRegistrationError>
AggregatableTriggerData::FromJSON(base::Value& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataWrongType);
  }

  auto key_piece = ParseKeyPiece(*dict);
  if (!key_piece.has_value()) {
    return base::unexpected(key_piece.error());
  }
  auto source_keys = ParseSourceKeys(*dict);
  if (!source_keys.has_value()) {
    return base::unexpected(source_keys.error());
  }
  auto filters = FilterPair::FromJSON(*dict);
  if (!filters.has_value()) {
    return base::unexpected(filters.error());
  }
  return AggregatableTriggerData(*key_piece, std::move(*source_keys),
                                 std::move(*filters));
}

AggregatableTriggerData::AggregatableTriggerData() = default;

AggregatableTriggerData::AggregatableTriggerData(absl::uint128 key_piece,
                                                 Keys source_keys,
                                                 FilterPair filters)
    : key_piece_(key_piece),
      source_keys_(std::move(source_keys)),
      filters_(std::move(filters)) {
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

base::Value::Dict AggregatableTriggerData::ToJson() const {
  base::Value::Dict dict;

  dict.Set(kKeyPiece, HexEncodeAggregationKey(key_piece_));

  SerializeSourceKeysIfNotEmpty(dict, source_keys_);

  filters_.SerializeIfNotEmpty(dict);

  return dict;
}

}  // namespace attribution_reporting
