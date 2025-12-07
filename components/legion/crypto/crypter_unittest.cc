// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/crypto/crypter.h"

#include <optional>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace legion {

// Test fixture for LegionCrypter tests.
class LegionCrypterTest : public testing::Test {
 protected:
  const std::array<uint8_t, 32> kKey1 = {1};
  const std::array<uint8_t, 32> kKey2 = {2};
};

// Tests that a message can be encrypted and then successfully decrypted.
TEST_F(LegionCrypterTest, EncryptDecrypt) {
  Crypter crypter1(kKey1, kKey2);
  Crypter crypter2(kKey2, kKey1);

  const std::vector<uint8_t> original_message = {3, 4, 5, 6};

  std::optional<std::vector<uint8_t>> message =
      crypter1.Encrypt(original_message);
  ASSERT_TRUE(message);
  ASSERT_NE(*message, original_message);

  std::optional<std::vector<uint8_t>> decrypted_message =
      crypter2.Decrypt(*message);
  ASSERT_TRUE(decrypted_message);
  EXPECT_EQ(*decrypted_message, original_message);
}

// Tests that an empty message can be encrypted and decrypted.
TEST_F(LegionCrypterTest, EncryptDecryptEmpty) {
  Crypter crypter1(kKey1, kKey2);
  Crypter crypter2(kKey2, kKey1);

  const std::vector<uint8_t> original_message;

  std::optional<std::vector<uint8_t>> message =
      crypter1.Encrypt(original_message);
  ASSERT_TRUE(message);
  // Encrypting an empty message should not be a no-op.
  ASSERT_NE(*message, original_message);

  std::optional<std::vector<uint8_t>> decrypted_message =
      crypter2.Decrypt(*message);
  ASSERT_TRUE(decrypted_message);
  EXPECT_EQ(*decrypted_message, original_message);
}

// Tests that decryption fails if the wrong key is used.
TEST_F(LegionCrypterTest, BadKey) {
  Crypter crypter1(kKey1, kKey2);
  std::array<uint8_t, 32> key3 = {3};

  Crypter crypter2(key3, this->kKey1);

  const std::vector<uint8_t> message = {3, 4, 5, 6};

  std::optional<std::vector<uint8_t>> encrypted = crypter1.Encrypt(message);
  ASSERT_TRUE(encrypted);

  EXPECT_FALSE(crypter2.Decrypt(*encrypted));
}

// Tests that decryption fails if the ciphertext is modified.
TEST_F(LegionCrypterTest, CorruptedCiphertext) {
  Crypter crypter1(kKey1, kKey2);
  Crypter crypter2(kKey2, kKey1);

  const std::vector<uint8_t> message = {3, 4, 5, 6};

  std::optional<std::vector<uint8_t>> encrypted = crypter1.Encrypt(message);
  ASSERT_TRUE(encrypted);
  (*encrypted)[0] ^= 1;

  EXPECT_FALSE(crypter2.Decrypt(*encrypted));
}

// Tests encryption and decryption for a range of message sizes to check the
// padding logic.
TEST_F(LegionCrypterTest, Padding) {
  for (size_t i = 0; i < 40; i++) {
    Crypter crypter1(kKey1, kKey2);
    Crypter crypter2(kKey2, kKey1);

    const std::vector<uint8_t> original_message(i, static_cast<uint8_t>(i));

    std::optional<std::vector<uint8_t>> message =
        crypter1.Encrypt(original_message);
    ASSERT_TRUE(message);
    if (!original_message.empty()) {
      ASSERT_NE(*message, original_message);
    }

    std::optional<std::vector<uint8_t>> decrypted_message =
        crypter2.Decrypt(*message);
    ASSERT_TRUE(decrypted_message);
    EXPECT_EQ(*decrypted_message, original_message);
  }
}

}  // namespace legion
