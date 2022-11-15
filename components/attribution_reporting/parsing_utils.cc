// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parsing_utils.h"

#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_util.h"
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

}  // namespace attribution_reporting
