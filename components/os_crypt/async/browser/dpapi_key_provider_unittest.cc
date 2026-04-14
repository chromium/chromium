// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/dpapi_key_provider.h"

#include <windows.h>

#include <dpapi.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/win/scoped_localalloc.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

namespace {

constexpr char kOsCryptEncryptedKeyPrefName[] = "os_crypt.encrypted_key";
constexpr uint8_t kDPAPIKeyPrefix[] = {'D', 'P', 'A', 'P', 'I'};

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
  void SetUp() override {
    prefs_.registry()->RegisterStringPref(kOsCryptEncryptedKeyPrefName, "");
  }

  void TearDown() override {
    if (expected_histogram_) {
      histograms_.ExpectBucketCount("OSCrypt.DPAPIProvider.Status",
                                    *expected_histogram_, 1);
    }
  }

  Encryptor GetInstanceSync(
      OSCryptAsync& factory,
      Encryptor::Option option = Encryptor::Option::kNone) {
    base::test::TestFuture<Encryptor> future;
    factory.GetInstance(future.GetCallback(), option);
    return future.Take();
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
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

class DPAPIKeyProviderTest : public DPAPIKeyProviderTestBase {
 protected:
  void SetUp() override {
    DPAPIKeyProviderTestBase::SetUp();
    InitDPAPIKey();
  }

  void InitDPAPIKey() {
    const size_t kKeySize = 32;  // AES256
    std::vector<uint8_t> key(kKeySize);
    base::RandBytes(key);

    std::string encrypted_key_data;
    ASSERT_TRUE(EncryptStringWithDPAPI(std::string(key.begin(), key.end()),
                                       encrypted_key_data));

    std::vector<uint8_t> encrypted_key;
    encrypted_key.insert_range(encrypted_key.end(), kDPAPIKeyPrefix);
    encrypted_key.insert_range(encrypted_key.end(), encrypted_key_data);

    prefs_.SetString(kOsCryptEncryptedKeyPrefName,
                     base::Base64Encode(encrypted_key));
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

    // Encryptor should be able to decrypt.
    const auto decrypted = encryptor.DecryptData(*ciphertext);
    EXPECT_TRUE(decrypted);
    EXPECT_EQ(*decrypted, "secrets");
  }
  {
    // Now, obtain a second encryptor but with the kEncryptSyncCompat option.
    Encryptor encryptor_with_option =
        GetInstanceSync(factory, Encryptor::Option::kEncryptSyncCompat);
    // This should now encrypt with DPAPIKeyProvider, compatible with OSCrypt
    // sync, but still contain both keys for decryption.
    const auto second_ciphertext =
        encryptor_with_option.EncryptString("moresecrets");
    ASSERT_TRUE(second_ciphertext);

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

  {
    Encryptor encryptor = GetInstanceWithDPAPI();
    ASSERT_TRUE(encryptor.EncryptString("oscryptsecrets", &ciphertext));
  }

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
    ASSERT_FALSE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    // In fact, without fallback, this just fails to decrypt.
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
    // Without OSCrypt/sync fallback, raw pre-m79 DPAPI ciphertext (no provider
    // tag) cannot be routed to a key and decryption should fail.
    EXPECT_FALSE(encryptor.DecryptString(dpapi_ciphertext, &plaintext, &flags));
    EXPECT_FALSE(flags.should_reencrypt);
  }
}

// Only test a few scenarios here, just to verify the error histogram is always
// logged.
TEST_F(DPAPIKeyProviderTest, OSCryptNotInit) {
  prefs_.ClearPref(kOsCryptEncryptedKeyPrefName);
  Encryptor encryptor = GetInstanceWithDPAPI();
  // Encryption is not available because no key was loaded.
  EXPECT_FALSE(encryptor.IsEncryptionAvailable());
  EXPECT_FALSE(encryptor.IsDecryptionAvailable());
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kKeyNotFound;
}

TEST_F(DPAPIKeyProviderTest, OSCryptBadKeyHeader) {
  prefs_.SetString(kOsCryptEncryptedKeyPrefName, "badkeybadkey");
  Encryptor encryptor = GetInstanceWithDPAPI();
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kInvalidKeyHeader;
}

TEST_F(DPAPIKeyProviderTestBase, NoOSCrypt) {
  Encryptor encryptor = GetInstanceWithDPAPI();
  EXPECT_FALSE(encryptor.IsEncryptionAvailable());
  EXPECT_FALSE(encryptor.IsDecryptionAvailable());
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kKeyNotFound;
}

TEST_F(DPAPIKeyProviderTest, DPAPIFailing) {
  // This first part obtains an encryptor with the DPAPI Key provider, then
  // encrypts some data with it.
  std::optional<std::vector<uint8_t>> ciphertext;
  {
    Encryptor encryptor = GetInstanceWithDPAPI();
    ciphertext = encryptor.EncryptString("secret");
    ASSERT_TRUE(ciphertext);
  }

  // Now, break the DPAPI key provider by storing a key that will not decrypt
  // with DPAPI.
  prefs_.SetString(kOsCryptEncryptedKeyPrefName,
                   base::Base64Encode("DPAPIBadKey"));
  expected_histogram_ = DPAPIKeyProvider::KeyStatus::kDPAPIDecryptFailure;
  {
    Encryptor encryptor = GetInstanceWithDPAPI();
    Encryptor::DecryptFlags flags;
    const auto decrypted = encryptor.DecryptData(*ciphertext, &flags);
    // Decryption should now fail as no key is available.
    ASSERT_FALSE(decrypted);
    ASSERT_TRUE(flags.temporarily_unavailable);
  }
}

}  // namespace os_crypt_async
