// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
absl::optional<absl::uint128> StringToAggregationKeyPiece(const std::string& s);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::string HexEncodeAggregationKey(absl::uint128);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool AggregationKeyIdHasValidLength(const std::string& key);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
absl::optional<uint64_t> ParseUint64(const base::Value::Dict& dict,
                                     base::StringPiece key);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
absl::optional<int64_t> ParseInt64(const base::Value::Dict& dict,
                                   base::StringPiece key);

int64_t ParsePriority(const base::Value::Dict& dict);

absl::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict);

bool ParseDebugReporting(const base::Value::Dict& dict);

void SerializeUint64(base::Value::Dict&, base::StringPiece key, uint64_t value);

void SerializeInt64(base::Value::Dict&, base::StringPiece key, int64_t value);

void SerializePriority(base::Value::Dict&, int64_t priority);

void SerializeDebugKey(base::Value::Dict&, absl::optional<uint64_t> debug_key);

void SerializeDebugReporting(base::Value::Dict&, bool debug_reporting);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
