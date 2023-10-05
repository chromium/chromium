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
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {
constexpr char kDebugKey[] = "debug_key";
constexpr char kDebugReporting[] = "debug_reporting";
constexpr char kDeduplicationKey[] = "deduplication_key";
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

bool ParseUint64(const base::Value::Dict& dict,
                 base::StringPiece key,
                 absl::optional<uint64_t>& out) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    out = absl::nullopt;
    return true;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    out = absl::nullopt;
    return false;
  }

  uint64_t parsed_val;
  out = base::StringToUint64(*str, &parsed_val)
            ? absl::make_optional(parsed_val)
            : absl::nullopt;
  return out.has_value();
}

bool ParseInt64(const base::Value::Dict& dict,
                base::StringPiece key,
                absl::optional<int64_t>& out) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    out = absl::nullopt;
    return true;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    out = absl::nullopt;
    return false;
  }

  int64_t parsed_val;
  out = base::StringToInt64(*str, &parsed_val) ? absl::make_optional(parsed_val)
                                               : absl::nullopt;
  return out.has_value();
}

bool ParsePriority(const base::Value::Dict& dict,
                   absl::optional<int64_t>& out) {
  return ParseInt64(dict, kPriority, out);
}

absl::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict) {
  absl::optional<uint64_t> debug_key;
  std::ignore = ParseUint64(dict, kDebugKey, debug_key);
  return debug_key;
}

bool ParseDeduplicationKey(const base::Value::Dict& dict,
                           absl::optional<uint64_t>& out) {
  return ParseUint64(dict, kDeduplicationKey, out);
}

bool ParseDebugReporting(const base::Value::Dict& dict) {
  return dict.FindBool(kDebugReporting).value_or(false);
}

base::expected<base::TimeDelta, mojom::SourceRegistrationError>
ParseLegacyDuration(const base::Value& value,
                    mojom::SourceRegistrationError error) {
  // Note: The full range of uint64 seconds cannot be represented in the
  // resulting `base::TimeDelta`, but this is fine because `base::Seconds()`
  // properly clamps out-of-bound values and because the Attribution
  // Reporting API itself clamps values to 30 days:
  // https://wicg.github.io/attribution-reporting-api/#valid-source-expiry-range

  if (absl::optional<int> int_value = value.GetIfInt()) {
    if (*int_value < 0) {
      return base::unexpected(error);
    }
    return base::Seconds(*int_value);
  }

  if (const std::string* str = value.GetIfString()) {
    uint64_t seconds;
    if (!base::StringToUint64(*str, &seconds)) {
      return base::unexpected(error);
    }
    return base::Seconds(seconds);
  }

  return base::unexpected(error);
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

void SerializeDeduplicationKey(base::Value::Dict& dict,
                               absl::optional<uint64_t> dedup_key) {
  if (dedup_key) {
    SerializeUint64(dict, kDeduplicationKey, *dedup_key);
  }
}

void SerializeTimeDeltaInSeconds(base::Value::Dict& dict,
                                 base::StringPiece key,
                                 base::TimeDelta value) {
  int64_t seconds = value.InSeconds();
  if (base::IsValueInRangeForNumericType<int>(seconds)) {
    dict.Set(key, static_cast<int>(seconds));
  } else {
    SerializeInt64(dict, key, seconds);
  }
}

}  // namespace attribution_reporting
