// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/fast_pair_decryption.h"

#include <algorithm>
#include <array>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_encryption.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_decryption {

std::array<uint8_t, kBlockByteSize> aes_key_bytes = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

class FastPairDecryptionTest : public testing::Test {};

TEST_F(FastPairDecryptionTest, ParseDecryptedResponse_Success) {
  std::vector<uint8_t> response_bytes;

  // Message type.
  response_bytes.push_back(0x01);

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  base::ranges::copy(address_bytes, std::back_inserter(response_bytes));

  // Random salt
  std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                 0x0D, 0x0E, 0x0F, 0x00};
  base::ranges::copy(salt, std::back_inserter(response_bytes));

  std::array<uint8_t, kBlockByteSize> response_bytes_array;
  std::copy_n(response_bytes.begin(), kBlockByteSize,
              response_bytes_array.begin());

  auto encrypted_bytes =
      fast_pair_encryption::EncryptBytes(aes_key_bytes, response_bytes_array);
  auto response = ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
  EXPECT_EQ(response->address_bytes, address_bytes);
  EXPECT_EQ(response->salt, salt);
}

TEST_F(FastPairDecryptionTest, ParseDecryptedResponse_Failure) {
  std::array<uint8_t, kBlockByteSize> response_bytes = {/*message_type=*/0x02,
                                                        /*address_bytes=*/0x02,
                                                        0x03,
                                                        0x04,
                                                        0x05,
                                                        0x06,
                                                        0x07,
                                                        /*salt=*/0x08,
                                                        0x09,
                                                        0x0A,
                                                        0x0B,
                                                        0x0C,
                                                        0x0D,
                                                        0x0E,
                                                        0x0F,
                                                        0x00};

  auto encrypted_bytes =
      fast_pair_encryption::EncryptBytes(aes_key_bytes, response_bytes);
  auto response = ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(response.has_value());
}

TEST_F(FastPairDecryptionTest, ParseDecryptedExtendedResponseOneAddr_Success) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairKeyboards},
      /*disabled_features=*/{});

  std::vector<uint8_t> response_bytes;

  // Message type.
  response_bytes.push_back(0x02);

  // Flags.
  uint8_t flags = 0x01;
  response_bytes.push_back(flags);

  // Num Addresses.
  uint8_t num_addresses = 0x01;
  response_bytes.push_back(num_addresses);

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
  base::ranges::copy(address_bytes, std::back_inserter(response_bytes));

  // Random salt
  std::array<uint8_t, 7> salt = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00};
  base::ranges::copy(salt, std::back_inserter(response_bytes));
  std::array<uint8_t, 9> expected_salt;
  expected_salt.fill(0);
  std::copy(salt.begin(), salt.end(), expected_salt.begin());

  std::array<uint8_t, kBlockByteSize> response_bytes_array;
  std::copy_n(response_bytes.begin(), kBlockByteSize,
              response_bytes_array.begin());

  auto encrypted_bytes =
      fast_pair_encryption::EncryptBytes(aes_key_bytes, response_bytes_array);
  auto response = ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->message_type,
            FastPairMessageType::kKeyBasedPairingExtendedResponse);
  EXPECT_TRUE(response->flags.has_value());
  EXPECT_EQ(response->flags.value(), flags);
  EXPECT_TRUE(response->num_addresses.has_value());
  EXPECT_EQ(response->num_addresses.value(), num_addresses);
  EXPECT_EQ(response->address_bytes, address_bytes);
  EXPECT_FALSE(response->secondary_address_bytes);
  EXPECT_EQ(response->salt, expected_salt);
}

TEST_F(FastPairDecryptionTest, ParseDecryptedExtendedResponseTwoAddr_Success) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairKeyboards},
      /*disabled_features=*/{});

  std::vector<uint8_t> response_bytes;

  // Message type.
  response_bytes.push_back(0x02);

  // Flags.
  uint8_t flags = 0x01;
  response_bytes.push_back(flags);

  // Num Addresses.
  uint8_t num_addresses = 0x02;
  response_bytes.push_back(num_addresses);

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
  base::ranges::copy(address_bytes, std::back_inserter(response_bytes));

  // Secondary address bytes.
  std::array<uint8_t, 6> secondary_address_bytes = {0x0A, 0x0B, 0x0C,
                                                    0x0D, 0x0E, 0x0F};
  base::ranges::copy(secondary_address_bytes,
                     std::back_inserter(response_bytes));

  // Random salt
  std::array<uint8_t, 1> salt = {0x10};
  base::ranges::copy(salt, std::back_inserter(response_bytes));
  std::array<uint8_t, 9> expected_salt;
  expected_salt.fill(0);
  std::copy(salt.begin(), salt.end(), expected_salt.begin());

  std::array<uint8_t, kBlockByteSize> response_bytes_array;
  std::copy_n(response_bytes.begin(), kBlockByteSize,
              response_bytes_array.begin());

  auto encrypted_bytes =
      fast_pair_encryption::EncryptBytes(aes_key_bytes, response_bytes_array);
  auto response = ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->message_type,
            FastPairMessageType::kKeyBasedPairingExtendedResponse);
  EXPECT_TRUE(response->flags.has_value());
  EXPECT_EQ(response->flags.value(), flags);
  EXPECT_TRUE(response->num_addresses.has_value());
  EXPECT_EQ(response->num_addresses.value(), num_addresses);
  EXPECT_EQ(response->address_bytes, address_bytes);
  EXPECT_TRUE(response->secondary_address_bytes);
  EXPECT_EQ(response->secondary_address_bytes.value(), secondary_address_bytes);
  EXPECT_EQ(response->salt, expected_salt);
}

TEST_F(FastPairDecryptionTest, ParseDecryptedPasskey_Success) {
  std::vector<uint8_t> passkey_bytes;

  // Message type.
  passkey_bytes.push_back(0x02);

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes.push_back(passkey >> 16);
  passkey_bytes.push_back(passkey >> 8);
  passkey_bytes.push_back(passkey);

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  base::ranges::copy(salt, std::back_inserter(passkey_bytes));

  std::array<uint8_t, kBlockByteSize> passkey_bytes_array;
  std::copy_n(passkey_bytes.begin(), kBlockByteSize,
              passkey_bytes_array.begin());

  auto encrypted_bytes =
      fast_pair_encryption::EncryptBytes(aes_key_bytes, passkey_bytes_array);
  auto decrypted_passkey =
      ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(decrypted_passkey.has_value());
  EXPECT_EQ(decrypted_passkey->message_type,
            FastPairMessageType::kSeekersPasskey);
  EXPECT_EQ(decrypted_passkey->passkey, passkey);
  EXPECT_EQ(decrypted_passkey->salt, salt);
}

TEST_F(FastPairDecryptionTest, ParseDecryptedPasskey_Failure) {
  std::array<uint8_t, kBlockByteSize> passkey_bytes = {/*message_type=*/0x04,
                                                       /*passkey=*/0x02,
                                                       0x03,
                                                       0x04,
                                                       /*salt=*/0x05,
                                                       0x06,
                                                       0x07,
                                                       0x08,
                                                       0x09,
                                                       0x0A,
                                                       0x0B,
                                                       0x0C,
                                                       0x0D,
                                                       0x0E,
                                                       0x0F,
                                                       0x0E};

  auto encrypted_bytes =
      fast_pair_encryption::EncryptBytes(aes_key_bytes, passkey_bytes);
  auto passkey = ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(passkey.has_value());
}

}  // namespace fast_pair_decryption
}  // namespace quick_pair
}  // namespace ash
