// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/keychain_key_provider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "crypto/apple/mock_keychain.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

class KeychainKeyProviderCompatTest : public ::testing::Test {
 protected:
  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }

  Encryptor GetEncryptor(crypto::apple::Keychain* keychain) {
    std::vector<
        std::pair<OSCryptAsync::Precedence, std::unique_ptr<KeyProvider>>>
        providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u,
        base::WrapUnique(new KeychainKeyProvider(keychain))));
    OSCryptAsync factory(std::move(providers));

    base::test::TestFuture<Encryptor> future;
    factory.GetInstance(future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC};
};

TEST_F(KeychainKeyProviderCompatTest, EncryptOldDecryptNew) {
  crypto::apple::MockKeychain mock_keychain;
  mock_keychain.set_find_generic_result(noErr);

  // 1. Encrypt with old sync backend.
  const std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString(plaintext, &ciphertext));

  // 2. Get an encryptor from the new async interface.
  Encryptor encryptor = GetEncryptor(&mock_keychain);

  // 3. Decrypt with the new async interface.
  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(KeychainKeyProviderCompatTest, EncryptNewDecryptOld) {
  crypto::apple::MockKeychain mock_keychain;
  mock_keychain.set_find_generic_result(noErr);

  // 1. Get an encryptor from the new async interface.
  Encryptor encryptor = GetEncryptor(&mock_keychain);

  // 2. Encrypt with the new async interface.
  const std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));

  // 3. Decrypt with old sync backend.
  std::string decrypted;
  EXPECT_TRUE(OSCrypt::DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

}  // namespace os_crypt_async
