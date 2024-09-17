// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <concepts>
#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace attribution_reporting {

class SuitableOrigin;

struct ParseError {
  friend bool operator==(ParseError, ParseError) = default;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<absl::uint128, ParseError> ParseAggregationKeyPiece(
    const base::Value&);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::string HexEncodeAggregationKey(absl::uint128);

template <typename T>
  requires(std::integral<T>)
constexpr T ValueOrZero(std::optional<T> value) {
  return value.value_or(0);
}

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::optional<uint64_t>, ParseError> ParseUint64(
    const base::Value::Dict&,
    std::string_view key);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::optional<int64_t>, ParseError> ParseInt64(
    const base::Value::Dict&,
    std::string_view key);

base::expected<int64_t, ParseError> ParsePriority(const base::Value::Dict&);

// Returns `debug_key` value as we do not need to fail the source registration
// if the value is invalid, see
// https://github.com/WICG/attribution-reporting-api/issues/793 for context.
std::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict);

// Returns false if `dict` contains `debug_reporting` key but the value is
// invalid, returns true otherwise.
[[nodiscard]] bool ParseDebugReporting(const base::Value::Dict& dict);

base::expected<std::optional<uint64_t>, ParseError> ParseDeduplicationKey(
    const base::Value::Dict&);

// The given value must be a non-negative `int`, or a non-negative `double`
// without a fractional part, or a string containing a base-10-formatted
// unsigned 64-bit integer. That value is interpreted as a number of seconds
// clamped to the given range.
base::expected<base::TimeDelta, ParseError> ParseLegacyDuration(
    const base::Value&,
    base::TimeDelta clamp_min,
    base::TimeDelta clamp_max);

// The given value must be an `int` or a `double` without a fractional part.
// That value is interpreted as a number of seconds. The only clamping applied
// is that of `base::TimeDelta` itself, which only affects extremely large
// `double` values that for the purposes of Attribution Reporting are
// effectively infinity and will be clamped or tolerated properly elsewhere.
base::expected<base::TimeDelta, ParseError> ParseDuration(const base::Value&);

base::expected<std::optional<SuitableOrigin>, ParseError>
ParseAggregationCoordinator(const base::Value::Dict&);

void SerializeUint64(base::Value::Dict&, std::string_view key, uint64_t value);

void SerializeInt64(base::Value::Dict&, std::string_view key, int64_t value);

void SerializePriority(base::Value::Dict&, int64_t priority);

void SerializeDebugKey(base::Value::Dict&, std::optional<uint64_t> debug_key);

void SerializeDebugReporting(base::Value::Dict&, bool debug_reporting);

void SerializeDeduplicationKey(base::Value::Dict&,
                               std::optional<uint64_t> dedup_key);

void SerializeTimeDeltaInSeconds(base::Value::Dict& dict,
                                 std::string_view key,
                                 base::TimeDelta value);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<int, ParseError> ParseInt(const base::Value&);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<uint32_t, ParseError> ParseUint32(const base::Value&);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<uint32_t, ParseError> ParsePositiveUint32(const base::Value&);

base::Value Uint32ToJson(uint32_t);

enum class StringSetError {
  kWrongType,
  kStringTooLong,
  kSetTooLong,
};

base::expected<base::flat_set<std::string>, StringSetError> ExtractStringSet(
    base::Value::List,
    size_t max_string_size,
    size_t max_set_size);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
