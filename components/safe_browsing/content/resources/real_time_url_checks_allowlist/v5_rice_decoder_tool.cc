// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "components/safe_browsing/core/browser/db/v5_hash_list_rice_decoder.h"
#include "components/safe_browsing/core/browser/db/v5_rice.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"

namespace {
constexpr size_t kFullHashSize = 32;
constexpr size_t kHashPrefixSize = 16;
}  // namespace

// Command line tool to parse a Safe Browsing v5 BatchGetHashListsResponse proto
// from standard input and decode the 256-bit Golomb-Rice delta encoded full
// hashes for the gca-32b list, outputting raw concatenated 16-byte prefixes to
// stdout. This reuses the same util that Desktop update requests use in live
// Chrome instances ('safe_browsing::v5_hash_list_rice_decoder').
int main(int argc, char* argv[]) {
  // Read the raw protobuf response bytes from standard input.
  std::vector<uint8_t> input_bytes((std::istreambuf_iterator<char>(std::cin)),
                                   std::istreambuf_iterator<char>());

  if (input_bytes.empty()) {
    std::cerr << "Empty input received on stdin" << std::endl;
    return 1;
  }

  safe_browsing::V5::BatchGetHashListsResponse response;
  if (!response.ParseFromArray(input_bytes.data(), input_bytes.size())) {
    std::cerr << "Failed to parse BatchGetHashListsResponse proto" << std::endl;
    return 1;
  }

  if (response.hash_lists_size() != 1) {
    std::cerr << "Expected 1 hash list in response, but got "
              << response.hash_lists_size() << std::endl;
    return 1;
  }

  const safe_browsing::V5::HashList& gca_list = response.hash_lists(0);
  if (gca_list.name() != "gca-32b") {
    std::cerr << "Expected hash list 'gca-32b', but got '" << gca_list.name()
              << "'" << std::endl;
    return 1;
  }

  if (gca_list.partial_update() || gca_list.has_compressed_removals()) {
    std::cerr << "Expected a full gca-32b hash list update, but received a "
                 "partial update"
              << std::endl;
    return 1;
  }

  if (!gca_list.has_additions_thirty_two_bytes()) {
    std::cerr << "gca-32b hash list does not contain additions_thirty_two_bytes"
              << std::endl;
    return 1;
  }

  safe_browsing::V5InputValidationResult validation_result =
      safe_browsing::v5_hash_list_rice_decoder::ValidateHashList(gca_list);
  if (validation_result != safe_browsing::V5InputValidationResult::kSuccess) {
    std::cerr << "Rice input validation failed with result code: "
              << static_cast<int>(validation_result) << std::endl;
    return 1;
  }

  std::string decoded_full_hashes;
  safe_browsing::V5DecodeResult decode_result =
      safe_browsing::v5_hash_list_rice_decoder::DecodeAdditions(
          gca_list, decoded_full_hashes);
  if (decode_result != safe_browsing::V5DecodeResult::kSuccess) {
    std::cerr << "Rice decoding failed with result code: "
              << static_cast<int>(decode_result) << std::endl;
    return 1;
  }

  if (decoded_full_hashes.size() % kFullHashSize != 0) {
    std::cerr << "Decoded full hashes size is not a multiple of 32 bytes"
              << std::endl;
    return 1;
  }

  std::string truncated_prefixes;
  truncated_prefixes.reserve((decoded_full_hashes.size() / kFullHashSize) *
                             kHashPrefixSize);
  for (size_t i = 0; i < decoded_full_hashes.size(); i += kFullHashSize) {
    truncated_prefixes.append(decoded_full_hashes, i, kHashPrefixSize);
  }

  std::cout.write(truncated_prefixes.data(), truncated_prefixes.size());
  return 0;
}
