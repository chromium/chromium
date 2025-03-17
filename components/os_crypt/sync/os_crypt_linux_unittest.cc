// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <array>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/os_crypt/sync/key_storage_linux.h"
#include "components/os_crypt/sync/os_crypt_mocker_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyStorageLinux> GetNullKeyStorage() {
  return nullptr;
}

class OSCryptLinuxTest : public testing::Test {
 public:
  OSCryptLinuxTest() = default;

  OSCryptLinuxTest(const OSCryptLinuxTest&) = delete;
  OSCryptLinuxTest& operator=(const OSCryptLinuxTest&) = delete;

  ~OSCryptLinuxTest() override = default;

  void SetUp() override {
    OSCryptMockerLinux::SetUp();
    OSCrypt::SetEncryptionPasswordForTesting("something");
  }

  void TearDown() override { OSCryptMockerLinux::TearDown(); }
};

TEST_F(OSCryptLinuxTest, VerifyV10) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  OSCrypt::SetEncryptionPasswordForTesting("peanuts");
  ASSERT_TRUE(OSCrypt::EncryptString(originaltext, &ciphertext));
  OSCrypt::SetEncryptionPasswordForTesting("not_peanuts");
  ciphertext = ciphertext.substr(3).insert(0, "v10");
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

TEST_F(OSCryptLinuxTest, VerifyV11) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  OSCrypt::SetEncryptionPasswordForTesting(std::string());
  ASSERT_TRUE(OSCrypt::EncryptString(originaltext, &ciphertext));
  ASSERT_EQ(ciphertext.substr(0, 3), "v11");
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

TEST_F(OSCryptLinuxTest, IsEncryptionAvailable) {
  EXPECT_TRUE(OSCrypt::IsEncryptionAvailable());
  OSCrypt::ClearCacheForTesting();
  // Mock the GetKeyStorage function.
  OSCrypt::UseMockKeyStorageForTesting(base::BindOnce(&GetNullKeyStorage));
  EXPECT_FALSE(OSCrypt::IsEncryptionAvailable());
}

TEST_F(OSCryptLinuxTest, SetRawEncryptionKey) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  // Encrypt with not_peanuts and save the raw encryption key.
  OSCrypt::SetEncryptionPasswordForTesting("not_peanuts");
  ASSERT_TRUE(OSCrypt::EncryptString(originaltext, &ciphertext));
  ASSERT_EQ(ciphertext.substr(0, 3), "v11");
  std::string raw_key = OSCrypt::GetRawEncryptionKey();
  ASSERT_FALSE(raw_key.empty());

  // Clear the cached encryption key.
  OSCrypt::ClearCacheForTesting();

  // Set the raw encryption key and make sure decryption works.
  OSCrypt::SetRawEncryptionKey(raw_key);
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

// Because of crbug.com/1195256, there might be data that is encrypted with an
// empty key. These should remain decryptable, even when a proper key is
// available.
TEST_F(OSCryptLinuxTest, DecryptWhenEncryptionKeyIsEmpty) {
  base::HistogramTester histogram_tester;
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  // Encrypt a value using "" as the key.
  OSCrypt::SetEncryptionPasswordForTesting("");
  ASSERT_TRUE(OSCrypt::EncryptString(originaltext, &ciphertext));

  // Set a proper encryption key.
  OSCrypt::ClearCacheForTesting();
  OSCrypt::SetEncryptionPasswordForTesting("key");
  // The text is decryptable.
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  EXPECT_EQ(originaltext, decipheredtext);
  histogram_tester.ExpectUniqueSample("OSCrypt.Linux.DecryptedWithEmptyKey",
                                      true, 1);
}

struct KeyDerivationKnownAnswer {
  const char* password;
  std::array<uint8_t, 16> answer;
};

TEST_F(OSCryptLinuxTest, KeyDerivationKnownAnswers) {
  // Known answers from PBKDF2-HMAC-SHA1 producing 128 bits of output with 1
  // round and salted with "saltysalt".
  // TODO(https://crbug.com/380912393): re-enable clang-format for this block.
  // clang-format off
  constexpr auto kCases = std::to_array<KeyDerivationKnownAnswer>({
    {
      // The hardcoded V10 obfuscation key.
      .password = "peanuts",
      .answer = {
        0xfd, 0x62, 0x1f, 0xe5, 0xa2, 0xb4, 0x02, 0x53,
        0x9d, 0xfa, 0x14, 0x7c, 0xa9, 0x27, 0x27, 0x78,
      },
    },
    {
      // The empty password fallback obfuscation key.
      .password = "",
      .answer = {
        0xd0, 0xd0, 0xec, 0x9c, 0x7d, 0x77, 0xd4, 0x3a,
        0xc5, 0x41, 0x87, 0xfa, 0x48, 0x18, 0xd1, 0x7f,
      },
    },
    {
      // An example V11 key generated from a random password.
      .password = "zxqfb",
      .answer = {
        0xb7, 0x56, 0x30, 0x74, 0x74, 0xb0, 0x0d, 0xa3,
        0x55, 0xf7, 0x73, 0xf0, 0x2f, 0x86, 0x1a, 0xe4,
      },
    },
  });
  // clang-format on

  for (const auto& c : kCases) {
    OSCrypt::SetEncryptionPasswordForTesting(c.password);
    std::string r = OSCrypt::GetRawEncryptionKey();
    EXPECT_EQ(c.answer, base::as_byte_span(r));
  }
}

TEST_F(OSCryptLinuxTest, EncryptDecryptKnownAnswers) {
  OSCrypt::SetEncryptionPasswordForTesting("zxqfb");
  const std::string kInput = "Hello, World! This is a longer string.";

  // The input string, encrypted using AES-128-CBC, with the "V11" version
  // prefix and with a PKCS#5 padding block at the end.
  // clang-format off
  constexpr auto kExpectedCiphertext = std::to_array<uint8_t>({
    0x76, 0x31, 0x31,
    0x77, 0x21, 0x00, 0x60, 0x87, 0x09, 0xe8, 0x94,
    0xde, 0x63, 0x4f, 0x80, 0x36, 0x40, 0xb7, 0xdd,
    0x80, 0xe1, 0x2c, 0x6f, 0x9a, 0xb6, 0x6b, 0xba,
    0x93, 0xaf, 0xec, 0xb5, 0x89, 0xe6, 0x12, 0x28,
    0xf0, 0x85, 0xa2, 0xbb, 0xf9, 0x30, 0x30, 0x87,
    0xf8, 0xd1, 0x10, 0x4e, 0x97, 0xe1, 0x75, 0x73,
  });
  // clang-format on

  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString(kInput, &ciphertext));
  EXPECT_EQ(kExpectedCiphertext, base::as_byte_span(ciphertext));

  std::string plaintext;
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &plaintext));
  EXPECT_EQ(kInput, plaintext);
}

}  // namespace
