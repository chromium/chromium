// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_data.h"

#include <stddef.h>

#include <limits>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

base::expected<absl::uint128, TriggerRegistrationError> ParseKeyPiece(
    const base::Value::Dict& registration) {
  const base::Value* v = registration.Find(kKeyPiece);
  if (!v) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataKeyPieceMissing);
  }

  return ParseAggregationKeyPiece(*v).transform_error([](ParseError) {
    return TriggerRegistrationError::kAggregatableTriggerDataKeyPieceInvalid;
  });
}

base::expected<AggregatableTriggerData::Keys, TriggerRegistrationError>
ParseSourceKeys(base::Value::Dict& registration) {
  base::Value* v = registration.Find(kSourceKeys);
  if (!v)
    return AggregatableTriggerData::Keys();

  base::Value::List* l = v->GetIfList();
  if (!l) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataSourceKeysInvalid);
  }

  return ExtractStringSet(
             std::move(*l),
             /*max_string_size=*/std::numeric_limits<size_t>::max(),
             /*max_set_size=*/std::numeric_limits<size_t>::max())
      .transform_error([](StringSetError) {
        return TriggerRegistrationError::
            kAggregatableTriggerDataSourceKeysInvalid;
      });
}

void SerializeSourceKeysIfNotEmpty(base::Value::Dict& dict,
                                   const AggregatableTriggerData::Keys& keys) {
  if (keys.empty())
    return;

  auto list = base::Value::List::with_capacity(keys.size());
  for (const std::string& key : keys) {
    list.Append(key);
  }
  dict.Set(kSourceKeys, std::move(list));
}

}  // namespace

// static
base::expected<AggregatableTriggerData, TriggerRegistrationError>
AggregatableTriggerData::FromJSON(base::Value& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableTriggerDataWrongType);
  }

  ASSIGN_OR_RETURN(auto key_piece, ParseKeyPiece(*dict));
  ASSIGN_OR_RETURN(auto source_keys, ParseSourceKeys(*dict));
  ASSIGN_OR_RETURN(auto filters, FilterPair::FromJSON(*dict));
  return AggregatableTriggerData(key_piece, std::move(source_keys),
                                 std::move(filters));
}

AggregatableTriggerData::AggregatableTriggerData() = default;

AggregatableTriggerData::AggregatableTriggerData(absl::uint128 key_piece,
                                                 Keys source_keys,
                                                 FilterPair filters)
    : key_piece_(key_piece),
      source_keys_(std::move(source_keys)),
      filters_(std::move(filters)) {
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
