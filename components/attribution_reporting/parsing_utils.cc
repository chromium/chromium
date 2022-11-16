// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parsing_utils.h"

#include <stdint.h>

#include <string>

#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

absl::optional<absl::uint128> StringToAggregationKeyPiece(
    const std::string& s) {
  if (!base::StartsWith(s, "0x", base::CompareCase::INSENSITIVE_ASCII))
    return absl::nullopt;

  absl::uint128 key_piece;
  if (!base::HexStringToUInt128(s, &key_piece))
    return absl::nullopt;

  return key_piece;
}

bool AggregationKeyIdHasValidLength(const std::string& key) {
  return key.size() <= kMaxBytesPerAggregationKeyId;
}

absl::optional<uint64_t> ParseUint64(const base::Value::Dict& dict,
                                     base::StringPiece key) {
  const std::string* s = dict.FindString(key);
  if (!s)
    return absl::nullopt;

  uint64_t value;
  return base::StringToUint64(*s, &value) ? absl::make_optional(value)
                                          : absl::nullopt;
}

absl::optional<int64_t> ParseInt64(const base::Value::Dict& dict,
                                   base::StringPiece key) {
  const std::string* s = dict.FindString(key);
  if (!s)
    return absl::nullopt;

  int64_t value;
  return base::StringToInt64(*s, &value) ? absl::make_optional(value)
                                         : absl::nullopt;
}

int64_t ParsePriority(const base::Value::Dict& dict) {
  return ParseInt64(dict, "priority").value_or(0);
}

absl::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict) {
  return ParseUint64(dict, "debug_key");
}

bool ParseDebugReporting(const base::Value::Dict& dict) {
  return dict.FindBool("debug_reporting").value_or(false);
}

}  // namespace attribution_reporting
