// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "components/os_crypt/key_storage_linux.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker_linux.h"
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
    os_crypt_mocker_linux_.SetUp();
    os_crypt_.SetEncryptionPasswordForTesting("something");
  }

  void TearDown() override { os_crypt_mocker_linux_.TearDown(); }

 protected:
  OSCrypt os_crypt_;
  OSCryptMockerLinux os_crypt_mocker_linux_{&os_crypt_};
};

TEST_F(OSCryptLinuxTest, VerifyV0) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  os_crypt_.SetEncryptionPasswordForTesting(std::string());
  ciphertext = originaltext;  // No encryption
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

TEST_F(OSCryptLinuxTest, VerifyV10) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  os_crypt_.SetEncryptionPasswordForTesting("peanuts");
  ASSERT_TRUE(os_crypt_.EncryptString(originaltext, &ciphertext));
  os_crypt_.SetEncryptionPasswordForTesting("not_peanuts");
  ciphertext = ciphertext.substr(3).insert(0, "v10");
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

TEST_F(OSCryptLinuxTest, VerifyV11) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  os_crypt_.SetEncryptionPasswordForTesting(std::string());
  ASSERT_TRUE(os_crypt_.EncryptString(originaltext, &ciphertext));
  ASSERT_EQ(ciphertext.substr(0, 3), "v11");
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

TEST_F(OSCryptLinuxTest, IsEncryptionAvailable) {
  EXPECT_TRUE(os_crypt_.IsEncryptionAvailable());
  os_crypt_.ClearCacheForTesting();
  // Mock the GetKeyStorage function.
  os_crypt_.UseMockKeyStorageForTesting(base::BindOnce(&GetNullKeyStorage));
  EXPECT_FALSE(os_crypt_.IsEncryptionAvailable());
}

TEST_F(OSCryptLinuxTest, SetRawEncryptionKey) {
  const std::string originaltext = "hello";
  std::string ciphertext;
  std::string decipheredtext;

  // Encrypt with not_peanuts and save the raw encryption key.
  os_crypt_.SetEncryptionPasswordForTesting("not_peanuts");
  ASSERT_TRUE(os_crypt_.EncryptString(originaltext, &ciphertext));
  ASSERT_EQ(ciphertext.substr(0, 3), "v11");
  std::string raw_key = os_crypt_.GetRawEncryptionKey();
  ASSERT_FALSE(raw_key.empty());

  // Clear the cached encryption key.
  os_crypt_.ClearCacheForTesting();

  // Set the raw encryption key and make sure decryption works.
  os_crypt_.SetRawEncryptionKey(raw_key);
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &decipheredtext));
  ASSERT_EQ(originaltext, decipheredtext);
}

}  // namespace
