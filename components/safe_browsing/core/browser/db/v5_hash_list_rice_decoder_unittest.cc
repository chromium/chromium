// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_hash_list_rice_decoder.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/safe_browsing/core/browser/db/v5_rice.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(V5HashListRiceDecoderTest, DecodeAdditions32Bit) {
  V5::HashList hash_list;
  auto* additions = hash_list.mutable_additions_four_bytes();
  additions->set_first_value(10);
  additions->set_rice_parameter(3);
  additions->set_entries_count(1);
  // First value: 0x0000000A (10)
  // Delta: 0x01 (1)
  // Expected output values: 0x0000000A, 0x0000000B

  // Set the correct encoded data for success.
  additions->set_encoded_data("\x02");
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  std::string raw_additions;
  EXPECT_EQ(
      V5DecodeResult::kSuccess,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, raw_additions));
  std::vector<uint8_t> expected = {0x00, 0x00, 0x00, 0x0A,
                                   0x00, 0x00, 0x00, 0x0B};
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(raw_additions));

  // Test failure propagation (empty encoded data).
  additions->set_encoded_data("");
  std::string failed_additions;
  EXPECT_EQ(
      V5DecodeResult::kRanOutOfBits,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, failed_additions));
}

TEST(V5HashListRiceDecoderTest, DecodeAdditions64Bit) {
  V5::HashList hash_list;
  auto* additions = hash_list.mutable_additions_eight_bytes();
  additions->set_first_value(4294967306ULL);
  additions->set_rice_parameter(35);
  additions->set_entries_count(1);
  // First value: 0x000000010000000A (4294967306)
  // Delta: 0x01 (1)
  // Expected output values: 0x000000010000000A, 0x000000010000000B

  // Set the correct encoded data for success.
  additions->set_encoded_data(std::string("\x02\x00\x00\x00\x00", 5));
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  std::string raw_additions;
  EXPECT_EQ(
      V5DecodeResult::kSuccess,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, raw_additions));
  std::vector<uint8_t> expected = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                                   0x00, 0x0A, 0x00, 0x00, 0x00, 0x01,
                                   0x00, 0x00, 0x00, 0x0B};
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(raw_additions));

  // Test failure propagation (empty encoded data).
  additions->set_encoded_data("");
  std::string failed_additions;
  EXPECT_EQ(
      V5DecodeResult::kRanOutOfBits,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, failed_additions));
}

TEST(V5HashListRiceDecoderTest, DecodeAdditions128Bit) {
  V5::HashList hash_list;
  auto* additions = hash_list.mutable_additions_sixteen_bytes();
  additions->set_first_value_hi(1);
  additions->set_first_value_lo(2);
  additions->set_rice_parameter(99);
  additions->set_entries_count(1);
  // First value: 0x00000000000000010000000000000002 (hi=1, lo=2)
  // Delta: 0x01 (1)
  // Expected output values:
  //   1: 0x00000000000000010000000000000002
  //   2: 0x00000000000000010000000000000003

  // Set the correct encoded data for success.
  std::string encoded_data(13, 0);
  encoded_data[0] = 0x02;
  additions->set_encoded_data(encoded_data);
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  std::string raw_additions;
  EXPECT_EQ(
      V5DecodeResult::kSuccess,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, raw_additions));
  std::vector<uint8_t> expected(32, 0);
  expected[7] = 0x01;
  expected[15] = 0x02;
  expected[23] = 0x01;
  expected[31] = 0x03;
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(raw_additions));

  // Test failure propagation (empty encoded data).
  additions->set_encoded_data("");
  std::string failed_additions;
  EXPECT_EQ(
      V5DecodeResult::kRanOutOfBits,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, failed_additions));
}

TEST(V5HashListRiceDecoderTest, DecodeAdditions256Bit) {
  V5::HashList hash_list;
  auto* additions = hash_list.mutable_additions_thirty_two_bytes();
  additions->set_first_value_first_part(1);
  additions->set_first_value_second_part(2);
  additions->set_first_value_third_part(3);
  additions->set_first_value_fourth_part(4);
  additions->set_rice_parameter(227);
  additions->set_entries_count(1);
  // First value parts: (0x1, 0x2, 0x3, 0x4) (each 64-bit)
  // Delta: 0x01 (1)
  // Expected output values:
  //   1: (0x1, 0x2, 0x3, 0x4)
  //   2: (0x1, 0x2, 0x3, 0x5)

  // Set the correct encoded data for success.
  std::string encoded_data(29, 0);
  encoded_data[0] = 0x02;
  additions->set_encoded_data(encoded_data);
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  std::string raw_additions;
  EXPECT_EQ(
      V5DecodeResult::kSuccess,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, raw_additions));
  std::vector<uint8_t> expected(64, 0);
  expected[7] = 0x01;
  expected[15] = 0x02;
  expected[23] = 0x03;
  expected[31] = 0x04;
  expected[39] = 0x01;
  expected[47] = 0x02;
  expected[55] = 0x03;
  expected[63] = 0x05;
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(raw_additions));

  // Test failure propagation (empty encoded data).
  additions->set_encoded_data("");
  std::string failed_additions;
  EXPECT_EQ(
      V5DecodeResult::kRanOutOfBits,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, failed_additions));
}

TEST(V5HashListRiceDecoderTest, DecodeRemovals) {
  V5::HashList hash_list;
  auto* removals = hash_list.mutable_compressed_removals();
  removals->set_first_value(10);
  removals->set_rice_parameter(3);
  removals->set_entries_count(1);
  // First value: 0x0A (10)
  // Delta: 0x01 (1)
  // Expected output values: 0x0A, 0x0B

  // Set the correct encoded data for success.
  removals->set_encoded_data("\x02");
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  std::vector<uint32_t> decoded_removals;
  EXPECT_EQ(V5DecodeResult::kSuccess, v5_hash_list_rice_decoder::DecodeRemovals(
                                          hash_list, decoded_removals));
  ASSERT_EQ(2u, decoded_removals.size());
  EXPECT_EQ(10u, decoded_removals[0]);
  EXPECT_EQ(11u, decoded_removals[1]);

  // Test failure propagation (empty encoded data).
  removals->set_encoded_data("");
  std::vector<uint32_t> failed_removals;
  EXPECT_EQ(
      V5DecodeResult::kRanOutOfBits,
      v5_hash_list_rice_decoder::DecodeRemovals(hash_list, failed_removals));
}

TEST(V5HashListRiceDecoderTest, DecodeAdditionsEmpty) {
  V5::HashList hash_list;
  std::string raw_additions;
  EXPECT_EQ(
      V5DecodeResult::kSuccess,
      v5_hash_list_rice_decoder::DecodeAdditions(hash_list, raw_additions));
  EXPECT_TRUE(raw_additions.empty());
}

TEST(V5HashListRiceDecoderTest, DecodeRemovalsEmpty) {
  V5::HashList hash_list;
  std::vector<uint32_t> decoded_removals;
  EXPECT_EQ(V5DecodeResult::kSuccess, v5_hash_list_rice_decoder::DecodeRemovals(
                                          hash_list, decoded_removals));
  EXPECT_TRUE(decoded_removals.empty());
}

TEST(V5HashListRiceDecoderTest, ValidateHashListBothAdditionsAndRemovals) {
  V5::HashList hash_list;

  // Set valid additions.
  auto* additions = hash_list.mutable_additions_four_bytes();
  additions->set_rice_parameter(3);
  additions->set_entries_count(10);

  // Set valid removals.
  auto* removals = hash_list.mutable_compressed_removals();
  removals->set_rice_parameter(3);
  removals->set_entries_count(10);

  EXPECT_EQ(V5InputValidationResult::kSuccess,
            v5_hash_list_rice_decoder::ValidateHashList(hash_list));
}

TEST(V5HashListRiceDecoderTest, ValidateHashListNoAdditionsOrRemovals) {
  V5::HashList hash_list;
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            v5_hash_list_rice_decoder::ValidateHashList(hash_list));
}

TEST(V5HashListRiceDecoderTest, ValidateHashListInvalid) {
  // Invalid 4-byte additions
  {
    V5::HashList hash_list;
    auto* additions = hash_list.mutable_additions_four_bytes();
    additions->set_rice_parameter(2);
    additions->set_entries_count(10);
    EXPECT_EQ(V5InputValidationResult::kRiceParameterTooSmall,
              v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  }

  // Invalid 8-byte additions
  {
    V5::HashList hash_list;
    auto* additions = hash_list.mutable_additions_eight_bytes();
    additions->set_rice_parameter(34);
    additions->set_entries_count(10);
    EXPECT_EQ(V5InputValidationResult::kRiceParameterTooSmall,
              v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  }

  // Invalid 16-byte additions
  {
    V5::HashList hash_list;
    auto* additions = hash_list.mutable_additions_sixteen_bytes();
    additions->set_rice_parameter(98);
    additions->set_entries_count(10);
    EXPECT_EQ(V5InputValidationResult::kRiceParameterTooSmall,
              v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  }

  // Invalid 32-byte additions
  {
    V5::HashList hash_list;
    auto* additions = hash_list.mutable_additions_thirty_two_bytes();
    additions->set_rice_parameter(226);
    additions->set_entries_count(10);
    EXPECT_EQ(V5InputValidationResult::kRiceParameterTooSmall,
              v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  }

  // Invalid removals
  {
    V5::HashList hash_list;
    auto* removals = hash_list.mutable_compressed_removals();
    removals->set_rice_parameter(2);
    removals->set_entries_count(10);
    EXPECT_EQ(V5InputValidationResult::kRiceParameterTooSmall,
              v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  }

  // Valid additions, but invalid removals
  {
    V5::HashList hash_list;
    auto* additions = hash_list.mutable_additions_four_bytes();
    additions->set_rice_parameter(3);
    additions->set_entries_count(10);
    auto* removals = hash_list.mutable_compressed_removals();
    removals->set_rice_parameter(2);
    removals->set_entries_count(10);
    EXPECT_EQ(V5InputValidationResult::kRiceParameterTooSmall,
              v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  }

  // Valid removals, but invalid additions
  {
    V5::HashList hash_list;
    auto* additions = hash_list.mutable_additions_four_bytes();
    additions->set_rice_parameter(2);
    additions->set_entries_count(10);
    auto* removals = hash_list.mutable_compressed_removals();
    removals->set_rice_parameter(3);
    removals->set_entries_count(10);
    EXPECT_EQ(V5InputValidationResult::kRiceParameterTooSmall,
              v5_hash_list_rice_decoder::ValidateHashList(hash_list));
  }
}

}  // namespace safe_browsing
