// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
absl::optional<absl::uint128> StringToAggregationKeyPiece(const std::string& s);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::string HexEncodeAggregationKey(absl::uint128);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool AggregationKeyIdHasValidLength(const std::string& key);

// Returns false if `dict` contains `key` but the value is invalid (e.g. not a
// string, negative), returns true otherwise.
[[nodiscard]] COMPONENT_EXPORT(ATTRIBUTION_REPORTING) bool ParseUint64(
    const base::Value::Dict& dict,
    base::StringPiece key,
    absl::optional<uint64_t>& out);

// Returns false if `dict` contains `key` but the value is invalid (e.g. not a
// string, int64 overflow), returns true otherwise.
[[nodiscard]] COMPONENT_EXPORT(ATTRIBUTION_REPORTING) bool ParseInt64(
    const base::Value::Dict& dict,
    base::StringPiece key,
    absl::optional<int64_t>& out);

// Returns false if `dict` contains `priority` key but the value is invalid,
// returns true otherwise.
[[nodiscard]] bool ParsePriority(const base::Value::Dict& dict,
                                 absl::optional<int64_t>& out);

// Returns `debug_key` value as we do not need to fail the source registration
// if the value is invalid, see
// https://github.com/WICG/attribution-reporting-api/issues/793 for context.
absl::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict);

// Returns false if `dict` contains `debug_reporting` key but the value is
// invalid, returns true otherwise.
[[nodiscard]] bool ParseDebugReporting(const base::Value::Dict& dict);

// Returns false if `dict` contains `deduplication_key` key but the value is
// invalid, returns true otherwise.
[[nodiscard]] bool ParseDeduplicationKey(const base::Value::Dict& dict,
                                         absl::optional<uint64_t>& out);

base::expected<base::TimeDelta, mojom::SourceRegistrationError>
ParseLegacyDuration(const base::Value& value,
                    mojom::SourceRegistrationError error);

void SerializeUint64(base::Value::Dict&, base::StringPiece key, uint64_t value);

void SerializeInt64(base::Value::Dict&, base::StringPiece key, int64_t value);

void SerializePriority(base::Value::Dict&, int64_t priority);

void SerializeDebugKey(base::Value::Dict&, absl::optional<uint64_t> debug_key);

void SerializeDebugReporting(base::Value::Dict&, bool debug_reporting);

void SerializeDeduplicationKey(base::Value::Dict&,
                               absl::optional<uint64_t> dedup_key);

void SerializeTimeDeltaInSeconds(base::Value::Dict& dict,
                                 base::StringPiece key,
                                 base::TimeDelta value);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
