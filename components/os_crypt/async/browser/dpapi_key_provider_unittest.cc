// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/dpapi_key_provider.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

// This class tests that DPAPIKeyProvider is forwards and backwards
// compatible with OSCrypt.
class DPAPIKeyProviderTest : public ::testing::Test {
 public:
  void SetUp() override {
    OSCrypt::RegisterLocalPrefs(prefs_.registry());
    OSCrypt::Init(&prefs_);
  }

  void TearDown() override {
    OSCrypt::ResetStateForTesting();
    histograms_.ExpectBucketCount("OSCrypt.DPAPIProvider.Status",
                                  expected_histogram_, 1);
  }

 protected:
  Encryptor GetInstanceSync(
      OSCryptAsync& factory,
      Encryptor::Option option = Encryptor::Option::kNone) {
    base::RunLoop run_loop;
    std::optional<Encryptor> encryptor;
    auto sub =
        factory.GetInstance(base::BindLambdaForTesting(
                                [&](Encryptor encryptor_param, bool success) {
                                  EXPECT_TRUE(success);
                                  encryptor.emplace(std::move(encryptor_param));
                                  run_loop.Quit();
                                }),
                            option);
    run_loop.Run();
    return std::move(*encryptor);
  }

  Encryptor GetInstanceWithDPAPI() {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u, std::make_unique<DPAPIKeyProvider>(&prefs_)));
    OSCryptAsync factory(std::move(providers));
    return GetInstanceSync(factory);
  }

  DPAPIKeyProvider::KeyStatus expected_histogram_ =
      DPAPIKeyProvider::KeyStatus::kSuccess;
  TestingPrefServiceSimple prefs_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histograms_;
};

TEST_F(DPAPIKeyProviderTest, Basic) {
  Encryptor encryptor = GetInstanceWithDPAPI();

  std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));

  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(DPAPIKeyProviderTest, DecryptOld) {
  Encryptor encryptor = GetInstanceWithDPAPI();

  std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString(plaintext, &ciphertext));

  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(DPAPIKeyProviderTest, EncryptForOld) {
  Encryptor encryptor = GetInstanceWithDPAPI();

  std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));

  std::string decrypted;
  EXPECT_TRUE(OSCrypt::DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

// Very small Key Provider that provides a random key.
class RandomKeyProvider : public KeyProvider {
 private:
  void GetKey(KeyCallback callback) final {
    std::vector<uint8_t> key(Encryptor::Key::kAES256GCMKeySize);
    base::RandBytes(key);
    std::move(callback).Run("_",
                            Encryptor::Key(key, mojom::Algorithm::kAES256GCM));
  }

  bool UseForEncryption() final { return true; }
  bool IsCompatibleWithOsCryptSync() final { return false; }
};

TEST_F(DPAPIKeyProviderTest, EncryptWithOptions) {
  std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
  providers.emplace_back(std::make_pair(
      /*precedence=*/10u, std::make_unique<DPAPIKeyProvider>(&prefs_)));
  // Random Key Provider will take precedence here.
  providers.emplace_back(std::make_pair(/*precedence=*/15u,
                                        std::make_unique<RandomKeyProvider>()));

  OSCryptAsync factory(std::move(providers));
  Encryptor encryptor = GetInstanceSync(factory);
  std::optional<std::vector<uint8_t>> ciphertext;
  {
    // This should use RandomKeyProvider.
    ciphertext = encryptor.EncryptString("secrets");
    ASSERT_TRUE(ciphertext);
    EXPECT_EQ(ciphertext->at(0), '_');
    std::string plaintext;
    // Fail, as it's encrypted with the '_' key provider.
    EXPECT_FALSE(OSCrypt::DecryptString(
        std::string(ciphertext->begin(), ciphertext->end()), &plaintext));

    // Encryptor should be able to decrypt.
    const auto decrypted = encryptor.DecryptData(*ciphertext);
    EXPECT_TRUE(decrypted);
    EXPECT_EQ(*decrypted, "secrets");
  }
  {
    // Now, obtain a second encryptor but with the kEncryptSyncCompat option.
    Encryptor encryptor_with_option =
        GetInstanceSync(factory, Encryptor::Option::kEncryptSyncCompat);
    // This should now encrypt with DPAPI key provider, compatible with OSCrypt
    // sync, but still contain both keys.
    const auto second_ciphertext =
        encryptor_with_option.EncryptString("moresecrets");
    ASSERT_TRUE(second_ciphertext);
    std::string plaintext;

    // First, test a decrypt using OSCrypt sync works.
    ASSERT_TRUE(OSCrypt::DecryptString(
        std::string(second_ciphertext->begin(), second_ciphertext->end()),
        &plaintext));
    EXPECT_EQ(plaintext, "moresecrets");

    // Now test both encryptors can decrypt both sets of ciphertext, regardless
    // of the option.
    {
      // First Encryptor with first ciphertext.
      const auto decrypted = encryptor.DecryptData(*ciphertext);
      ASSERT_TRUE(decrypted);
      EXPECT_EQ(*decrypted, "secrets");
    }
    {
      // First Encryptor with second ciphertext.
      const auto decrypted = encryptor.DecryptData(*second_ciphertext);
      ASSERT_TRUE(decrypted);
      EXPECT_EQ(*decrypted, "moresecrets");
    }
    {
      // Second encryptor (with option) with first ciphertext.
      const auto decrypted = encryptor_with_option.DecryptData(*ciphertext);
      ASSERT_TRUE(decrypted);
      EXPECT_EQ(*decrypted, "secrets");
    }
    {
      // Second encryptor (with option) with second ciphertext.
      const auto decrypted =
          encryptor_with_option.DecryptData(*second_ciphertext);
      ASSERT_TRUE(decrypted);
      EXPECT_EQ(*decrypted, "moresecrets");
    }
  }
}
// Only test a few scenarios here, just to verify the error histogram is always
// logged.
TEST_F(DPAPIKeyProviderTest, OSCryptNotInit) {
  prefs_.ClearPref("os_crypt.encrypted_key");
  Encryptor encryptor = GetInstanceWithDPAPI();
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kKeyNotFound;
}

TEST_F(DPAPIKeyProviderTest, OSCryptBadKeyHeader) {
  prefs_.SetString("os_crypt.encrypted_key", "badkeybadkey");
  Encryptor encryptor = GetInstanceWithDPAPI();
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kInvalidKeyHeader;
}

}  // namespace os_crypt_async
