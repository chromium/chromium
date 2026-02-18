// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/verification_key_utils.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "components/private_ai/attestation/server_verification_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {
namespace {

// Tests for ParseTinkSignature.
TEST(VerificationKeyUtilsTest, ParseTinkSignature) {
  // A valid Tink signature prefix (0x01) followed by a 4-byte key ID and
  // the raw signature.
  std::vector<uint8_t> valid_signature = {0x01, 0x12, 0x34, 0x56, 0x78,
                                          0xDE, 0xAD, 0xBE, 0xEF};
  auto parsed = ParseTinkSignature(valid_signature);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(0x12345678u, parsed->first);
  ASSERT_EQ(4u, parsed->second.size());
  EXPECT_EQ(0xDE, parsed->second[0]);
}

TEST(VerificationKeyUtilsTest, ParseTinkSignatureInvalidSize) {
  std::vector<uint8_t> short_signature = {0x01, 0x12, 0x34, 0x56};
  auto parsed = ParseTinkSignature(short_signature);
  EXPECT_FALSE(parsed.has_value());
}

TEST(VerificationKeyUtilsTest, ParseTinkSignatureKeyIdTooShort) {
  // The signature has a prefix but is too short to contain a full key ID.
  std::vector<uint8_t> short_signature = {0x01, 0x12, 0x34};
  auto parsed = ParseTinkSignature(short_signature);
  EXPECT_FALSE(parsed.has_value());
}

TEST(VerificationKeyUtilsTest, ParseTinkSignatureUnsupportedPrefix) {
  std::vector<uint8_t> invalid_prefix_signature = {0x02, 0x12, 0x34, 0x56, 0x78,
                                                   0xDE, 0xAD, 0xBE, 0xEF};
  auto parsed = ParseTinkSignature(invalid_prefix_signature);
  EXPECT_FALSE(parsed.has_value());
}

TEST(VerificationKeyUtilsTest, ParseTinkSignaturePrefixZero) {
  // A valid Tink signature prefix (0x00) followed by a 4-byte key ID and
  // the raw signature.
  std::vector<uint8_t> valid_signature = {0x00, 0x12, 0x34, 0x56, 0x78,
                                          0xDE, 0xAD, 0xBE, 0xEF};
  auto parsed = ParseTinkSignature(valid_signature);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(0x12345678u, parsed->first);
  ASSERT_EQ(4u, parsed->second.size());
  EXPECT_EQ(0xDE, parsed->second[0]);
}

// Tests for LoadVerificationKeys.
TEST(VerificationKeyUtilsTest, LoadVerificationKeysSuccess) {
  auto key_map = LoadVerificationKeys(GetServerVerificationKey());
  EXPECT_FALSE(key_map.empty());
}

TEST(VerificationKeyUtilsTest, LoadVerificationKeysAutopush) {
  auto key_map = LoadVerificationKeys(GetAutopushKeysForTesting());
  EXPECT_FALSE(key_map.empty());
}

TEST(VerificationKeyUtilsTest, LoadVerificationKeysDev) {
  auto key_map = LoadVerificationKeys(GetDevKeysForTesting());
  EXPECT_FALSE(key_map.empty());
}

TEST(VerificationKeyUtilsTest, LoadVerificationKeysStaging) {
  auto key_map = LoadVerificationKeys(GetStagingKeysForTesting());
  EXPECT_FALSE(key_map.empty());
}

TEST(VerificationKeyUtilsTest, LoadVerificationKeysEmpty) {
  EXPECT_TRUE(LoadVerificationKeys(base::span<const ProcessedKey>()).empty());
}

TEST(VerificationKeyUtilsTest, LoadVerificationKeysInvalidKeyData) {
  const ProcessedKey invalid_keys[] = {
      {/*id=*/123,
       /*output_prefix_type=*/OutputPrefixType::LEGACY,
       /*x=*/
       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
       /*y=*/
       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"}};
  ASSERT_DEATH(LoadVerificationKeys(invalid_keys), "");
}

}  // namespace
}  // namespace private_ai
