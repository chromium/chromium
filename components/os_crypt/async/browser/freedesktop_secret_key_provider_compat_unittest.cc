// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/freedesktop_secret_key_provider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/posix_key_provider.h"
#include "components/os_crypt/sync/key_storage_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

namespace {

constexpr char kSecretKey[] = "the_secret_key";
constexpr char kPlaintext[] = "the_secret_plaintext";

class OSCryptMockerLinux : public KeyStorageLinux {
 public:
  OSCryptMockerLinux() = default;

  OSCryptMockerLinux(const OSCryptMockerLinux&) = delete;
  OSCryptMockerLinux& operator=(const OSCryptMockerLinux&) = delete;

  ~OSCryptMockerLinux() override = default;

  bool Init() override { return true; }
  std::optional<std::string> GetKeyImpl() override { return std::nullopt; }
};

std::unique_ptr<KeyStorageLinux> CreateNewMock() {
  return std::make_unique<OSCryptMockerLinux>();
}

}  // namespace

// This class tests that FreedesktopSecretKeyProvider is forwards and backwards
// compatible with OSCrypt.
class FreedesktopSecretKeyProviderCompatTest : public ::testing::Test {
 protected:
  Encryptor GetEncryptorInstance(bool v11) {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    if (v11) {
      auto provider = std::make_unique<FreedesktopSecretKeyProvider>(
          "gnome-libsecret", "", nullptr);
      provider->secret_for_testing_ = kSecretKey;
      providers.emplace_back(0, std::move(provider));
    } else {
      providers.emplace_back(0, std::make_unique<PosixKeyProvider>());
    }
    OSCryptAsync factory(std::move(providers));

    base::test::TestFuture<Encryptor> future;
    factory.GetInstance(future.GetCallback(), Encryptor::Option::kNone);
    return future.Take();
  }

  void TearDown() override { OSCrypt::ClearCacheForTesting(); }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(FreedesktopSecretKeyProviderCompatTest, IsAvailable) {
  OSCrypt::SetEncryptionPasswordForTesting(kSecretKey);
  Encryptor encryptor = GetEncryptorInstance(/*v11=*/true);

  ASSERT_TRUE(encryptor.IsEncryptionAvailable());
  ASSERT_TRUE(encryptor.IsDecryptionAvailable());
}

TEST_F(FreedesktopSecretKeyProviderCompatTest, DecryptOldV11) {
  OSCrypt::SetEncryptionPasswordForTesting(kSecretKey);
  Encryptor encryptor = GetEncryptorInstance(/*v11=*/true);

  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString(kPlaintext, &ciphertext));
  EXPECT_TRUE(base::StartsWith(ciphertext, "v11"));

  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(kPlaintext, decrypted);
}

TEST_F(FreedesktopSecretKeyProviderCompatTest, EncryptForOldV11) {
  OSCrypt::SetEncryptionPasswordForTesting(kSecretKey);
  Encryptor encryptor = GetEncryptorInstance(/*v11=*/true);

  std::string ciphertext;
  ASSERT_TRUE(encryptor.EncryptString(kPlaintext, &ciphertext));
  EXPECT_TRUE(base::StartsWith(ciphertext, "v11"));

  std::string decrypted;
  EXPECT_TRUE(OSCrypt::DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(kPlaintext, decrypted);
}

TEST_F(FreedesktopSecretKeyProviderCompatTest, DecryptOldV10) {
  OSCrypt::UseMockKeyStorageForTesting(base::BindOnce(&CreateNewMock));
  Encryptor encryptor = GetEncryptorInstance(/*v11=*/false);

  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString(kPlaintext, &ciphertext));
  EXPECT_TRUE(base::StartsWith(ciphertext, "v10"));

  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(kPlaintext, decrypted);
}

TEST_F(FreedesktopSecretKeyProviderCompatTest, EncryptForOldV10) {
  OSCrypt::UseMockKeyStorageForTesting(base::BindOnce(&CreateNewMock));
  Encryptor encryptor = GetEncryptorInstance(/*v11=*/false);

  std::string ciphertext;
  ASSERT_TRUE(encryptor.EncryptString(kPlaintext, &ciphertext));
  EXPECT_TRUE(base::StartsWith(ciphertext, "v10"));

  std::string decrypted;
  EXPECT_TRUE(OSCrypt::DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(kPlaintext, decrypted);
}

}  // namespace os_crypt_async
