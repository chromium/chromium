// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_hash_list_rice_decoder.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace safe_browsing::v5_hash_list_rice_decoder {

V5DecodeResult DecodeAdditions(const V5::HashList& hash_list,
                               std::string& raw_additions) {
  CHECK(raw_additions.empty());

  auto decode = [&raw_additions](const auto& additions,
                                 const auto& first_value) {
    return V5RiceDecoder::DecodePrefixes(
        first_value, additions.rice_parameter(), additions.entries_count(),
        base::as_byte_span(additions.encoded_data()), &raw_additions);
  };

  switch (hash_list.compressed_additions_case()) {
    case V5::HashList::kAdditionsFourBytes: {
      const auto& additions = hash_list.additions_four_bytes();
      return decode(additions, additions.first_value());
    }
    case V5::HashList::kAdditionsEightBytes: {
      const auto& additions = hash_list.additions_eight_bytes();
      return decode(additions, additions.first_value());
    }
    case V5::HashList::kAdditionsSixteenBytes: {
      const auto& additions = hash_list.additions_sixteen_bytes();
      return decode(additions, absl::MakeUint128(additions.first_value_hi(),
                                                 additions.first_value_lo()));
    }
    case V5::HashList::kAdditionsThirtyTwoBytes: {
      const auto& additions = hash_list.additions_thirty_two_bytes();
      return decode(
          additions,
          v5_rice_utils::Uint256(
              absl::MakeUint128(additions.first_value_first_part(),
                                additions.first_value_second_part()),
              absl::MakeUint128(additions.first_value_third_part(),
                                additions.first_value_fourth_part())));
    }
    case V5::HashList::COMPRESSED_ADDITIONS_NOT_SET:
      return V5DecodeResult::kSuccess;
  }
}

V5DecodeResult DecodeRemovals(const V5::HashList& hash_list,
                              std::vector<uint32_t>& decoded_removals) {
  CHECK(decoded_removals.empty());

  if (!hash_list.has_compressed_removals()) {
    return V5DecodeResult::kSuccess;
  }

  const V5::RiceDeltaEncoded32Bit& removals = hash_list.compressed_removals();
  return V5RiceDecoder::DecodeIntegers<uint32_t>(
      removals.first_value(), removals.rice_parameter(),
      removals.entries_count(), base::as_byte_span(removals.encoded_data()),
      &decoded_removals);
}

namespace {

V5InputValidationResult ValidateAdditions(const V5::HashList& hash_list) {
  switch (hash_list.compressed_additions_case()) {
    case V5::HashList::kAdditionsFourBytes: {
      const auto& additions = hash_list.additions_four_bytes();
      return V5RiceInputValidator::Validate<uint32_t>(
          additions.rice_parameter(), additions.entries_count());
    }
    case V5::HashList::kAdditionsEightBytes: {
      const auto& additions = hash_list.additions_eight_bytes();
      return V5RiceInputValidator::Validate<uint64_t>(
          additions.rice_parameter(), additions.entries_count());
    }
    case V5::HashList::kAdditionsSixteenBytes: {
      const auto& additions = hash_list.additions_sixteen_bytes();
      return V5RiceInputValidator::Validate<absl::uint128>(
          additions.rice_parameter(), additions.entries_count());
    }
    case V5::HashList::kAdditionsThirtyTwoBytes: {
      const auto& additions = hash_list.additions_thirty_two_bytes();
      return V5RiceInputValidator::Validate<v5_rice_utils::Uint256>(
          additions.rice_parameter(), additions.entries_count());
    }
    case V5::HashList::COMPRESSED_ADDITIONS_NOT_SET:
      return V5InputValidationResult::kSuccess;
  }
}

V5InputValidationResult ValidateRemovals(const V5::HashList& hash_list) {
  if (hash_list.has_compressed_removals()) {
    const auto& removals = hash_list.compressed_removals();
    return V5RiceInputValidator::Validate<uint32_t>(removals.rice_parameter(),
                                                    removals.entries_count());
  }
  return V5InputValidationResult::kSuccess;
}

}  // namespace

V5InputValidationResult ValidateHashList(const V5::HashList& hash_list) {
  V5InputValidationResult additions_result = ValidateAdditions(hash_list);
  if (additions_result != V5InputValidationResult::kSuccess) {
    return additions_result;
  }
  return ValidateRemovals(hash_list);
}

}  // namespace safe_browsing::v5_hash_list_rice_decoder
