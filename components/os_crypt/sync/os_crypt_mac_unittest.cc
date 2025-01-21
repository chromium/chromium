// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gtest/include/gtest/gtest.h"

class OSCryptMacTest : public ::testing::Test {
 public:
  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }
};

TEST_F(OSCryptMacTest, KnownAnswers) {
  // This plaintext is deliberately more than one AES block long.
  constexpr auto kPlaintext = std::to_array<uint8_t>({
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
      0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
      0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  });

  // 3-byte obfuscation prefix ("v10"), two ciphertext blocks, then one PKCS#5
  // padding block. Originally derived via:
  // 1. k = PBKDF2-HMAC-SHA1("mock_password", "saltysalt", 1003);
  // 2. iv = ' ' x 16 (ie: a string of 16 spaces)
  // 2. c = AES-128-CBC(plaintext, k, iv);
  constexpr auto kExpectedCiphertext = std::to_array<uint8_t>({
      0x76, 0x31, 0x30, 0xbf, 0x08, 0x6d, 0x20, 0x56, 0x86, 0x1a, 0x80,
      0xde, 0x82, 0x5f, 0xc9, 0x35, 0x86, 0x86, 0x30, 0x64, 0x4f, 0x2c,
      0xa1, 0x87, 0x45, 0x02, 0x13, 0xae, 0x66, 0x81, 0xb4, 0xd6, 0x43,
      0xd1, 0x9b, 0x25, 0x81, 0xc8, 0x5c, 0x88, 0x78, 0xc1, 0xbc, 0x97,
      0xe7, 0x26, 0xa1, 0x0e, 0x51, 0xea, 0x77,
  });

  // The known answers below were computed using the hardcoded encryption
  // password supplied by MockAppleKeychain; if that mock password or the key
  // derivation method are ever changed the known answers need to be recomputed.
  // This assert is here to avoid having a difficult-to-debug decryption failure
  // later in the test in this case.
  ASSERT_EQ(crypto::MockAppleKeychain().GetEncryptionPassword(),
            "mock_password")
      << "Mock keychain password is different than expected. If you changed it,"
         " you need to recompute the known answers in this test.";

  std::string ciphertext;
  ASSERT_TRUE(OSCryptImpl::GetInstance()->EncryptString(
      std::string(base::as_string_view(kPlaintext)), &ciphertext));
  // Everything is converted to a span for equality checking because it produces
  // better error messages when there are mismatches.
  EXPECT_EQ(base::as_byte_span(kExpectedCiphertext),
            base::as_byte_span(ciphertext));

  std::string plaintext;
  ASSERT_TRUE(OSCryptImpl::GetInstance()->DecryptString(ciphertext, &plaintext));
  EXPECT_EQ(base::as_byte_span(kPlaintext), base::as_byte_span(plaintext));
}

TEST_F(OSCryptMacTest, SetAndGetRaw) {
  OSCryptImpl oscrypt1;
  OSCryptImpl oscrypt2;

  oscrypt1.UseMockKeychainForTesting(true);
  oscrypt2.UseMockKeychainForTesting(true);

  oscrypt2.SetRawEncryptionKey(oscrypt1.GetRawEncryptionKey());
  EXPECT_GT(oscrypt1.GetRawEncryptionKey().size(), 0u);
  EXPECT_EQ(oscrypt1.GetRawEncryptionKey(), oscrypt2.GetRawEncryptionKey());
}
