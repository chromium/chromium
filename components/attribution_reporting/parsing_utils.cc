// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parsing_utils.h"

#include <stdint.h>

#include <sstream>
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

namespace {
constexpr char kDebugKey[] = "debug_key";
constexpr char kDebugReporting[] = "debug_reporting";
constexpr char kPriority[] = "priority";
}  // namespace

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

std::string HexEncodeAggregationKey(absl::uint128 value) {
  std::ostringstream out;
  out << "0x";
  out.setf(out.hex, out.basefield);
  out << value;
  return out.str();
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
  return ParseInt64(dict, kPriority).value_or(0);
}

absl::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict) {
  return ParseUint64(dict, kDebugKey);
}

bool ParseDebugReporting(const base::Value::Dict& dict) {
  return dict.FindBool(kDebugReporting).value_or(false);
}

void SerializeUint64(base::Value::Dict& dict,
                     base::StringPiece key,
                     uint64_t value) {
  dict.Set(key, base::NumberToString(value));
}

void SerializeInt64(base::Value::Dict& dict,
                    base::StringPiece key,
                    int64_t value) {
  dict.Set(key, base::NumberToString(value));
}

void SerializePriority(base::Value::Dict& dict, int64_t priority) {
  SerializeInt64(dict, kPriority, priority);
}

void SerializeDebugKey(base::Value::Dict& dict,
                       absl::optional<uint64_t> debug_key) {
  if (debug_key) {
    SerializeUint64(dict, kDebugKey, *debug_key);
  }
}

void SerializeDebugReporting(base::Value::Dict& dict, bool debug_reporting) {
  dict.Set(kDebugReporting, debug_reporting);
}

}  // namespace attribution_reporting
