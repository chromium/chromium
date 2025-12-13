// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/keychain_key_provider.h"

#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "crypto/apple/mock_keychain.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

class KeychainKeyProviderTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC};
};

TEST_F(KeychainKeyProviderTest, GetKey_Success) {
  crypto::apple::MockKeychain mock_keychain;
  mock_keychain.set_find_generic_result(noErr);

  KeychainKeyProvider provider(&mock_keychain);
  base::test::TestFuture<const std::string&,
                         base::expected<Encryptor::Key, KeyProvider::KeyError>>
      future;
  provider.GetKey(future.GetCallback());

  auto& [tag, key_result] = future.Get();
  EXPECT_EQ(tag, "v10");
  ASSERT_TRUE(key_result.has_value());
  EXPECT_FALSE(mock_keychain.called_add_generic());

  // Known answer test for key derivation.
  // This value is from PBKDF2-HMAC-SHA1("mock_password", "saltysalt", 1003).
  const std::vector<uint8_t> expected_key = {0xAF, 0x0F, 0x76, 0x2A, 0xAF, 0x6D,
                                             0x7D, 0x11, 0x58, 0x1B, 0x7A, 0xA8,
                                             0xCE, 0x72, 0x18, 0xDE};
  EXPECT_EQ(key_result.value().key_, expected_key);
}

TEST_F(KeychainKeyProviderTest, GetKey_NotFound) {
  crypto::apple::MockKeychain mock_keychain;
  mock_keychain.set_find_generic_result(errSecItemNotFound);

  KeychainKeyProvider provider(&mock_keychain);
  base::test::TestFuture<const std::string&,
                         base::expected<Encryptor::Key, KeyProvider::KeyError>>
      future;
  provider.GetKey(future.GetCallback());

  auto& [tag, key_result] = future.Get();
  EXPECT_EQ(tag, "v10");
  ASSERT_TRUE(key_result.has_value());
  EXPECT_TRUE(mock_keychain.called_add_generic());
}

TEST_F(KeychainKeyProviderTest, GetKey_Failure_AuthFailed) {
  crypto::apple::MockKeychain mock_keychain;
  mock_keychain.set_find_generic_result(errSecAuthFailed);

  KeychainKeyProvider provider(&mock_keychain);
  base::test::TestFuture<const std::string&,
                         base::expected<Encryptor::Key, KeyProvider::KeyError>>
      future;
  provider.GetKey(future.GetCallback());

  auto& [tag, key_result] = future.Get();
  EXPECT_EQ(tag, "v10");
  ASSERT_FALSE(key_result.has_value());
  EXPECT_EQ(key_result.error(), KeyProvider::KeyError::kTemporarilyUnavailable);
  EXPECT_FALSE(mock_keychain.called_add_generic());
}

TEST_F(KeychainKeyProviderTest, GetKey_Failure_OtherError) {
  crypto::apple::MockKeychain mock_keychain;
  mock_keychain.set_find_generic_result(errSecNotAvailable);

  KeychainKeyProvider provider(&mock_keychain);
  base::test::TestFuture<const std::string&,
                         base::expected<Encryptor::Key, KeyProvider::KeyError>>
      future;
  provider.GetKey(future.GetCallback());

  auto& [tag, key_result] = future.Get();
  EXPECT_EQ(tag, "v10");
  ASSERT_FALSE(key_result.has_value());
  EXPECT_EQ(key_result.error(), KeyProvider::KeyError::kTemporarilyUnavailable);
  EXPECT_FALSE(mock_keychain.called_add_generic());
}

}  // namespace os_crypt_async
