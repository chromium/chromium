// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/dpapi_key_provider.h"

#include <windows.h>

#include <dpapi.h>

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
#include "base/win/scoped_localalloc.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

namespace {

// Utility function to encrypt data using the raw DPAPI interface.
bool EncryptStringWithDPAPI(const std::string& plaintext,
                            std::string& ciphertext) {
  DATA_BLOB input = {};
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());

  BOOL result = FALSE;
  DATA_BLOB output = {};
  result =
      ::CryptProtectData(&input, /*szDataDescr=*/L"",
                         /*pOptionalEntropy=*/nullptr, /*pvReserved=*/nullptr,
                         /*pPromptStruct=*/nullptr, /*dwFlags=*/0, &output);
  if (!result) {
    return false;
  }

  auto local_alloc = base::win::TakeLocalAlloc(output.pbData);

  static_assert(sizeof(std::string::value_type) == 1);

  ciphertext.assign(
      reinterpret_cast<std::string::value_type*>(local_alloc.get()),
      output.cbData);

  return true;
}

}  // namespace
// This class tests that DPAPIKeyProvider is forwards and backwards
// compatible with OSCrypt.
class DPAPIKeyProviderTestBase : public ::testing::Test {
 protected:
  void TearDown() override {
    if (expected_histogram_) {
      histograms_.ExpectBucketCount("OSCrypt.DPAPIProvider.Status",
                                    *expected_histogram_, 1);
    }
  }

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

  TestingPrefServiceSimple prefs_;
  std::optional<DPAPIKeyProvider::KeyStatus> expected_histogram_ =
      DPAPIKeyProvider::KeyStatus::kSuccess;

 private:
  base::HistogramTester histograms_;
  base::test::TaskEnvironment task_environment_;
};

class DPAPIKeyProviderTest : public DPAPIKeyProviderTestBase {
 protected:
  void SetUp() override {
    OSCrypt::RegisterLocalPrefs(prefs_.registry());
    OSCrypt::Init(&prefs_);
  }

  void TearDown() override {
    OSCrypt::ResetStateForTesting();
    DPAPIKeyProviderTestBase::TearDown();
  }
};

TEST_F(DPAPIKeyProviderTest, Basic) {
  Encryptor encryptor = GetInstanceWithDPAPI();

  ASSERT_TRUE(encryptor.IsEncryptionAvailable());
  ASSERT_TRUE(encryptor.IsDecryptionAvailable());

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
 public:
  RandomKeyProvider(bool use_for_encryption = true)
      : use_for_encryption_(use_for_encryption) {}

 private:
  void GetKey(KeyCallback callback) final {
    std::vector<uint8_t> key(Encryptor::Key::kAES256GCMKeySize);
    base::RandBytes(key);
    std::move(callback).Run("_",
                            Encryptor::Key(key, mojom::Algorithm::kAES256GCM));
  }

  bool UseForEncryption() final { return use_for_encryption_; }
  bool IsCompatibleWithOsCryptSync() final { return false; }

  const bool use_for_encryption_;
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

TEST_F(DPAPIKeyProviderTest, ShouldReencrypt) {
  std::string ciphertext;
  // This test re-initializes the DPAPIKeyProvider a few times, so ignore
  // histogram results. These histograms are tested elsewhere.
  expected_histogram_ = std::nullopt;

  ASSERT_TRUE(OSCrypt::EncryptString("oscryptsecrets", &ciphertext));

  {
    auto encryptor = GetInstanceWithDPAPI();
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    // This should not require re-encryption because the only provider
    // registered, the DPAPI one, successfully decrypted the data, even though
    // it was originally encrypted by OSCrypt Sync.
    EXPECT_FALSE(flags.should_reencrypt);
  }

  {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/15u, std::make_unique<RandomKeyProvider>()));

    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    // This decryption used OSCrypt sync fallback, but there was a key provider
    // registered for encryption, so the data should be re-encrypted for the new
    // key provider.
    EXPECT_TRUE(flags.should_reencrypt);
  }

  {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/15u,
        std::make_unique<RandomKeyProvider>(/*use_for_encryption=*/false)));

    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    // This decryption used OSCrypt sync fallback, and the only key provider
    // registered did not say it should encrypt, so this should not re-encrypt.
    EXPECT_FALSE(flags.should_reencrypt);
  }

  {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u, std::make_unique<DPAPIKeyProvider>(&prefs_)));
    providers.emplace_back(std::make_pair(
        /*precedence=*/15u, std::make_unique<RandomKeyProvider>()));

    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    // This decryption used the DPAPI key provider, but there is also a
    // RandomKeyProvider registered so data should be re-encrypted.
    EXPECT_TRUE(flags.should_reencrypt);
  }

  {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u, std::make_unique<DPAPIKeyProvider>(&prefs_)));
    providers.emplace_back(std::make_pair(
        /*precedence=*/15u,
        std::make_unique<RandomKeyProvider>(/*use_for_encryption=*/false)));

    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    // This decryption used the DPAPI key provider. The RandomKeyProvider is not
    // to be used for encryption, so the data can remain encrypted OSCrypt Sync
    // compatible.
    EXPECT_FALSE(flags.should_reencrypt);
  }

  {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u, std::make_unique<DPAPIKeyProvider>(&prefs_)));
    providers.emplace_back(std::make_pair(
        /*precedence=*/5u, std::make_unique<RandomKeyProvider>()));

    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    // In this test, both key providers are enabled for encryption but the
    // RandomKeyProvider is a lower precedence than the DPAPI key provider, so
    // will not be the default for encryption, therefore since the data is
    // already encrypted with DPAPI, a re-encryption should not be requested.
    EXPECT_FALSE(flags.should_reencrypt);
  }

  {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u, std::make_unique<DPAPIKeyProvider>(&prefs_)));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    std::string dpapi_ciphertext;
    ASSERT_TRUE(EncryptStringWithDPAPI("dpapisecret", dpapi_ciphertext));
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(dpapi_ciphertext, &plaintext, &flags));
    // In this test, a re-encryption should be requested, because the data was
    // encrypted using very old (pre-m79) DPAPI.
    EXPECT_TRUE(flags.should_reencrypt);
  }
}

// Only test a few scenarios here, just to verify the error histogram is always
// logged.
TEST_F(DPAPIKeyProviderTest, OSCryptNotInit) {
  prefs_.ClearPref("os_crypt.encrypted_key");
  Encryptor encryptor = GetInstanceWithDPAPI();
  // Encryption is available because OSCrypt sync already initialized before the
  // test fixture invalidated the key for the DPAPI Key Provider, and so while
  // the encryptor has no keyring, it delegates successfully to OSCrypt sync.
  EXPECT_TRUE(encryptor.IsEncryptionAvailable());
  EXPECT_TRUE(encryptor.IsDecryptionAvailable());
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kKeyNotFound;
}

TEST_F(DPAPIKeyProviderTest, OSCryptBadKeyHeader) {
  prefs_.SetString("os_crypt.encrypted_key", "badkeybadkey");
  Encryptor encryptor = GetInstanceWithDPAPI();
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kInvalidKeyHeader;
}

TEST_F(DPAPIKeyProviderTestBase, NoOSCrypt) {
  Encryptor encryptor = GetInstanceWithDPAPI();
  // Compare with DPAPIKeyProviderTest.OSCryptNotInit above: Encryption is not
  // available for this test because OSCrypt was never initialized and so the
  // encryptor has no key, and OSCrypt::IsEncryptionAvailable is also returning
  // false.
  EXPECT_FALSE(encryptor.IsEncryptionAvailable());
  EXPECT_FALSE(encryptor.IsDecryptionAvailable());
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kKeyNotFound;
}

}  // namespace os_crypt_async
