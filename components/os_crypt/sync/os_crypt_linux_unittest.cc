// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/os_crypt/sync/key_storage_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
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

TEST_F(OSCryptLinuxTest, VerifyV0) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  OSCrypt::SetEncryptionPasswordForTesting(std::string());
  ciphertext = originaltext;  // No encryption
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

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

}  // namespace
