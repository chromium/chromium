// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "crypto/hkdf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/key_storage_linux.h"
#endif

namespace os_crypt_async {

class OSCryptAsyncTest : public ::testing::Test {
 protected:
  using ProviderList =
      std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>>;

  Encryptor GetInstanceSync(
      OSCryptAsync& factory,
      Encryptor::Option option = Encryptor::Option::kNone) {
    base::test::TestFuture<Encryptor> future;
    factory.GetInstance(future.GetCallback(), option);
    return future.Take();
  }

  // Simulate a 'locked' OSCrypt keychain on platforms that need it, which makes
  // OSCrypt::IsEncryptionAvailable return false, without hitting a CHECK on
  // Linux. Note this is different from using the full OSCryptMocker, because in
  // this state, no key is available for encryption. Returns a
  // ScopedClosureRunner that will reset the behavior back to default when it
  // goes out of scope.
  [[nodiscard]] static std::optional<base::ScopedClosureRunner>
  MaybeSimulateLockedKeyChain() {
#if BUILDFLAG(IS_LINUX)
    OSCrypt::UseMockKeyStorageForTesting(base::BindOnce(
        []() -> std::unique_ptr<KeyStorageLinux> { return nullptr; }));
    return std::nullopt;
#elif BUILDFLAG(IS_APPLE)
    OSCrypt::SetKeychainForTesting(OSCrypt::MockLockedKeychain());
    return base::ScopedClosureRunner(
        base::BindOnce([]() { OSCrypt::SetKeychainForTesting(nullptr); }));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    OSCrypt::SetEncryptionAvailableForTesting(/*available=*/false);
    return base::ScopedClosureRunner(base::BindOnce([]() {
      OSCrypt::SetEncryptionAvailableForTesting(/*available=*/std::nullopt);
    }));
#else
    return std::nullopt;
#endif
  }

  base::test::TaskEnvironment task_environment_;
};

class TestKeyProvider : public KeyProvider {
 public:
  TestKeyProvider(const std::string& name,
                  bool use_for_encryption,
                  bool compatible_with_os_crypt_sync = false)
      : name_(name),
        use_for_encryption_(use_for_encryption),
        compatible_with_os_crypt_sync_(compatible_with_os_crypt_sync) {}

 protected:
  TestKeyProvider()
      : name_("TEST"),
        use_for_encryption_(true),
        compatible_with_os_crypt_sync_(false) {}

  Encryptor::Key GenerateKey() {
    // Make the key derive from the name to ensure different providers have
    // different keys.
    std::string key = crypto::HkdfSha256(name_, "salt", "info",
                                         Encryptor::Key::kAES256GCMKeySize);
    return Encryptor::Key(std::vector<uint8_t>(key.begin(), key.end()),
                          mojom::Algorithm::kAES256GCM);
  }

  const std::string name_;

 private:
  void GetKey(KeyCallback callback) override {
    std::move(callback).Run(name_, GenerateKey());
  }

  bool UseForEncryption() override { return use_for_encryption_; }
  bool IsCompatibleWithOsCryptSync() override {
    return compatible_with_os_crypt_sync_;
  }

  const bool use_for_encryption_;
  const bool compatible_with_os_crypt_sync_;
};

TEST_F(OSCryptAsyncTest, EncryptHeader) {
  const std::string kTestProviderName("TEST");
  ProviderList providers;
  providers.emplace_back(
      std::make_pair(10u, std::make_unique<TestKeyProvider>(
                              kTestProviderName, /*use_for_encryption=*/true)));
  OSCryptAsync factory(std::move(providers));
  Encryptor encryptor = GetInstanceSync(factory);

  auto ciphertext = encryptor.EncryptString("secrets");
  ASSERT_TRUE(std::equal(kTestProviderName.cbegin(), kTestProviderName.cend(),
                         ciphertext->cbegin()));
}

TEST_F(OSCryptAsyncTest, TwoProvidersBothEnabled) {
  std::optional<std::vector<uint8_t>> ciphertext;
  {
    const std::string kFooProviderName("FOO");
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u, std::make_unique<TestKeyProvider>(
                                kFooProviderName, /*use_for_encryption=*/true));
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("BAR", /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);

    ciphertext = encryptor.EncryptString("secrets");
    ASSERT_TRUE(ciphertext);
    // The higher of the two providers should have been picked for data
    // encryption.
    EXPECT_TRUE(std::equal(kFooProviderName.cbegin(), kFooProviderName.cend(),
                           ciphertext->cbegin()));
  }
  // Check that provider precedence does not matter for decrypt.
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("FOO", /*use_for_encryption=*/true));
    // BAR is the preferred provider since it has the higher precedence.
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<TestKeyProvider>("BAR", /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);

    auto plaintext = encryptor.DecryptData(*ciphertext);
    // The correct provider based on the encrypted data header should have been
    // picked for data decryption.
    ASSERT_TRUE(plaintext);
    EXPECT_EQ("secrets", *plaintext);
  }
  // Check that order of providers does not affect which one is chosen for
  // encrypt operations.
  {
    const std::string kFooProviderName("FOO");
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("BAR", /*use_for_encryption=*/true));
    providers.emplace_back(
        /*precedence=*/10u, std::make_unique<TestKeyProvider>(
                                kFooProviderName, /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);

    ciphertext = encryptor.EncryptString("secrets");
    ASSERT_TRUE(ciphertext);
    // The higher of the two providers should have been picked for data
    // encryption.
    EXPECT_TRUE(std::equal(kFooProviderName.cbegin(), kFooProviderName.cend(),
                           ciphertext->cbegin()));
  }
}

TEST_F(OSCryptAsyncTest, TwoProvidersOneEnabled) {
  std::optional<std::vector<uint8_t>> ciphertext;
  {
    const std::string kBarProviderName("BAR");
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<TestKeyProvider>("FOO", /*use_for_encryption=*/false));
    providers.emplace_back(
        /*precedence=*/5u, std::make_unique<TestKeyProvider>(
                               kBarProviderName, /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);

    ciphertext = encryptor.EncryptString("secrets");
    ASSERT_TRUE(ciphertext);
    // Despite FOO being higher than BAR, BAR is chosen for encryption because
    // FOO is not enabled for encryption.
    EXPECT_TRUE(std::equal(kBarProviderName.cbegin(), kBarProviderName.cend(),
                           ciphertext->cbegin()));
  }
  // Check that even with no enabled providers, data can still be decrypted by
  // any registered provider.
  {
    ProviderList providers;
    // Neither is enabled for encrypt.
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("FOO", /*use_for_encryption=*/false));
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<TestKeyProvider>("BAR", /*use_for_encryption=*/false));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);

    auto plaintext = encryptor.DecryptData(*ciphertext);
    // The correct provider based on the encrypted data header should have been
    // picked for data decryption.
    ASSERT_TRUE(plaintext);
    EXPECT_EQ("secrets", *plaintext);
  }
}

class OSCryptAsyncTestSwapped
    : public OSCryptAsyncTest,
      public ::testing::WithParamInterface</*switched=*/bool> {};

TEST_P(OSCryptAsyncTestSwapped, EncryptorOption) {
  std::string first_provider_name("TEST");
  std::string second_provider_name("BLAH");

  // This tests std::map ordering does not matter.
  if (GetParam()) {
    first_provider_name = "BLAH";
    second_provider_name = "TEST";
  }

  std::optional<std::vector<uint8_t>> first_ciphertext, second_ciphertext;
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<TestKeyProvider>(first_provider_name,
                                          /*use_for_encryption=*/true));
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>(
            second_provider_name, /*use_for_encryption=*/true,
            /*compatible_with_os_crypt_sync=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);

    first_ciphertext = encryptor.EncryptString("secrets");
    ASSERT_TRUE(first_ciphertext);
    // First provider should be picked, because it has a higher precedence than
    // the second.
    EXPECT_TRUE(std::equal(first_provider_name.cbegin(),
                           first_provider_name.cend(),
                           first_ciphertext->cbegin()));

    // Now obtain an encryptor with a compatibility option.
    Encryptor encryptor_compat =
        GetInstanceSync(factory, Encryptor::Option::kEncryptSyncCompat);
    second_ciphertext = encryptor_compat.EncryptString("secrets");
    ASSERT_TRUE(second_ciphertext);
    // Should be encrypted with second key now.
    EXPECT_TRUE(std::equal(second_provider_name.cbegin(),
                           second_provider_name.cend(),
                           second_ciphertext->cbegin()));
  }
  // Check that with just second provider, data can still be decrypted.
  {
    auto cleanup = MaybeSimulateLockedKeyChain();
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>(second_provider_name,
                                          /*use_for_encryption=*/false));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);

    // The only provider has indicated that it is not to be used for encryption,
    // so encryption should not be available, as OSCrypt fallback is not
    // available.
    ASSERT_FALSE(encryptor.IsEncryptionAvailable());
    // Decryption is possible, as long as the data is encrypted with the second
    // key.
    ASSERT_TRUE(encryptor.IsDecryptionAvailable());

    auto plaintext = encryptor.DecryptData(*second_ciphertext);
    // The correct provider based on the encrypted data header should have been
    // picked for data decryption.
    ASSERT_TRUE(plaintext);
    EXPECT_EQ("secrets", *plaintext);

    // The first data that was encrypted with v20 cannot be decrypted.
    auto failing_plaintext = encryptor.DecryptData(*first_ciphertext);
    EXPECT_FALSE(failing_plaintext);
  }
  // Test also that if there are multiple key providers with
  // compatible_with_os_crypt_sync then the highest precedence is picked.
  {
    ProviderList providers;
    providers.emplace_back(/*precedence=*/10u,
                           std::make_unique<TestKeyProvider>(
                               first_provider_name, /*use_for_encryption=*/true,
                               /*compatible_with_os_crypt_sync=*/true));
    providers.emplace_back(/*precedence=*/8u,
                           std::make_unique<TestKeyProvider>(
                               "FOO", /*use_for_encryption=*/true,
                               /*compatible_with_os_crypt_sync=*/false));
    providers.emplace_back(/*precedence=*/5u,
                           std::make_unique<TestKeyProvider>(
                               "BAR", /*use_for_encryption=*/true,
                               /*compatible_with_os_crypt_sync=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor =
        GetInstanceSync(factory, Encryptor::Option::kEncryptSyncCompat);
    const auto ciphertext = encryptor.EncryptString("secrets");
    ASSERT_TRUE(ciphertext);
    // Should be encrypted with first provider - it's the highest precedence
    // provider that indicates it's compatible with OSCrypt sync.
    EXPECT_TRUE(std::equal(first_provider_name.cbegin(),
                           first_provider_name.cend(), ciphertext->cbegin()));
  }
  // Just in case, test that order doesn't matter here, although this is also
  // tested elsewhere.
  {
    ProviderList providers;
    providers.emplace_back(/*precedence=*/5u,
                           std::make_unique<TestKeyProvider>(
                               "BAR", /*use_for_encryption=*/true,
                               /*compatible_with_os_crypt_sync=*/true));
    providers.emplace_back(/*precedence=*/8u,
                           std::make_unique<TestKeyProvider>(
                               "FOO", /*use_for_encryption=*/true,
                               /*compatible_with_os_crypt_sync=*/false));
    providers.emplace_back(/*precedence=*/10u,
                           std::make_unique<TestKeyProvider>(
                               first_provider_name, /*use_for_encryption=*/true,
                               /*compatible_with_os_crypt_sync=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor =
        GetInstanceSync(factory, Encryptor::Option::kEncryptSyncCompat);
    const auto ciphertext = encryptor.EncryptString("secrets");
    ASSERT_TRUE(ciphertext);
    // Should be encrypted with first provider - it's the highest precedence
    // provider that indicates it's compatible with OSCrypt sync.
    EXPECT_TRUE(std::equal(first_provider_name.cbegin(),
                           first_provider_name.cend(), ciphertext->cbegin()));
  }
}

INSTANTIATE_TEST_SUITE_P(, OSCryptAsyncTestSwapped, ::testing::Bool());

class SlowTestKeyProvider : public TestKeyProvider {
 public:
  explicit SlowTestKeyProvider(base::TimeDelta sleep_time) {}

 private:
  void GetKey(KeyCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](KeyCallback callback, Encryptor::Key key,
               const std::string& name) {
              std::move(callback).Run(name, std::move(key));
            },
            std::move(callback), GenerateKey(), name_),
        sleep_time_);
  }

  const base::TimeDelta sleep_time_;
};

// This test verifies that GetInstanceAsync can correctly handle multiple queued
// requests for an instance for a slow init.
TEST_F(OSCryptAsyncTest, MultipleCalls) {
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<SlowTestKeyProvider>(base::Seconds(1)));
  OSCryptAsync factory(std::move(providers));

  size_t calls = 0;
  const size_t kExpectedCalls = 10;
  base::RunLoop run_loop;
  for (size_t call = 0; call < kExpectedCalls; call++) {
    factory.GetInstance(
        base::BindLambdaForTesting([&calls, &run_loop](Encryptor encryptor) {
          calls++;
          if (calls == kExpectedCalls) {
            run_loop.Quit();
          }
        }));
  }
  run_loop.Run();
  EXPECT_EQ(calls, kExpectedCalls);
}

TEST_F(OSCryptAsyncTest, TestOSCryptAsyncInterface) {
  auto os_crypt = GetTestOSCryptAsyncForTesting();
  auto encryptor = GetInstanceSync(*os_crypt);
  auto ciphertext = encryptor.EncryptString("testsecrets");
  ASSERT_TRUE(ciphertext);
  {
    auto decrypted = encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ(*decrypted, "testsecrets");
  }
  {
    // Verify that all encryptors returned by the test OSCryptAsync instance use
    // the same keys.
    auto second_encryptor = GetInstanceSync(*os_crypt);
    auto decrypted = second_encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ(*decrypted, "testsecrets");
  }
  {
    // Verify that the key used by the encryptor returned from the testing
    // instance indicates compatibility with OSCrypt Sync. This avoids all tests
    // having to install the OSCrypt mocker in every fixture.
    const auto os_crypt_compat_encryptor =
        GetInstanceSync(*os_crypt, Encryptor::Option::kEncryptSyncCompat);
    {
      Encryptor::DecryptFlags flags;
      const auto decrypted =
          os_crypt_compat_encryptor.DecryptData(*ciphertext, &flags);
      ASSERT_TRUE(decrypted);
      // Switching from kNone to kEncryptSyncCompat should result in the data
      // needing to be re-encrypted.
      EXPECT_TRUE(flags.should_reencrypt);
      EXPECT_EQ(*decrypted, "testsecrets");
    }
    {
      // Encrypt with the OSCrypt Sync compat one, then decrypt with the new
      // one, which should signal again that the data needs to be re-encrypted.
      const auto os_crypt_compat_ciphertext =
          os_crypt_compat_encryptor.EncryptString("moresecret");
      ASSERT_TRUE(os_crypt_compat_ciphertext);
      Encryptor::DecryptFlags flags;
      const auto decrypted =
          encryptor.DecryptData(*os_crypt_compat_ciphertext, &flags);
      ASSERT_TRUE(decrypted);
      EXPECT_TRUE(flags.should_reencrypt);
      EXPECT_EQ(*decrypted, "moresecret");
    }
  }
  {
    auto another_encryptor =
        GetInstanceSync(*os_crypt, Encryptor::Option::kEncryptSyncCompat);
    auto decrypted = another_encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ(*decrypted, "testsecrets");
  }
}

TEST_F(OSCryptAsyncTest, TestEncryptorInterface) {
  auto encryptor = GetTestEncryptorForTesting();
  auto ciphertext = encryptor.EncryptString("testsecrets");
  ASSERT_TRUE(ciphertext);
  auto decrypted = encryptor.DecryptData(*ciphertext);
  ASSERT_TRUE(decrypted);
  EXPECT_EQ(*decrypted, "testsecrets");
}

TEST_F(OSCryptAsyncTest, TestEncryptorIsEncryptionAvailable) {
  auto encryptor = GetTestEncryptorForTesting();

  EXPECT_TRUE(encryptor.IsDecryptionAvailable());
  encryptor.set_decryption_available_for_testing(false);
  EXPECT_FALSE(encryptor.IsDecryptionAvailable());

  encryptor.set_decryption_available_for_testing(std::nullopt);
  EXPECT_TRUE(encryptor.IsDecryptionAvailable());

  EXPECT_TRUE(encryptor.IsEncryptionAvailable());
  encryptor.set_encryption_available_for_testing(false);
  EXPECT_FALSE(encryptor.IsEncryptionAvailable());

  encryptor.set_encryption_available_for_testing(std::nullopt);
  EXPECT_TRUE(encryptor.IsEncryptionAvailable());
}

class FailingKeyProvider : public TestKeyProvider {
 public:
  FailingKeyProvider(KeyProvider::KeyError reason, const std::string& name)
      : reason_(reason), name_(name) {}

 private:
  void GetKey(KeyCallback callback) override {
    std::move(callback).Run(name_, base::unexpected(reason_));
  }

  const KeyProvider::KeyError reason_;
  const std::string name_;
};

// Some tests require a working OSCrypt.
class OSCryptAsyncTestWithOSCrypt : public OSCryptAsyncTest {
 protected:
  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override {
    OSCryptMocker::TearDown();
#if BUILDFLAG(IS_WIN)
    OSCrypt::ResetStateForTesting();
#endif  // BUILDFLAG(IS_WIN)
  }
};

// This test merely verifies that OSCryptAsync can operate with no key providers
// and return a valid Encryptor with no keys, and that it can interop with
// OSCrypt. The rest of the encryption tests for this mode are located in
// encryptor_unittest.cc.
TEST_F(OSCryptAsyncTestWithOSCrypt, Empty) {
  base::HistogramTester histograms;
  ProviderList providers;
  OSCryptAsync factory(std::move(providers));
  Encryptor encryptor = GetInstanceSync(factory);
  {
    std::string ciphertext;
    EXPECT_TRUE(OSCrypt::EncryptString("secrets", &ciphertext));
    std::string plaintext;
    EXPECT_TRUE(encryptor.DecryptString(ciphertext, &plaintext));
    EXPECT_EQ("secrets", plaintext);
  }
  {
    const auto ciphertext = encryptor.EncryptString("moresecrets");
    ASSERT_TRUE(ciphertext.has_value());
    std::string plaintext;
    EXPECT_TRUE(OSCrypt::DecryptString(
        std::string(ciphertext->begin(), ciphertext->end()), &plaintext));
    EXPECT_EQ("moresecrets", plaintext);
  }
  histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount", 0, 1);
  histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount.Available", 0, 1);
  histograms.ExpectBucketCount(
      "OSCrypt.EncryptorKeyCount.TemporarilyUnavailable", 0, 1);
  histograms.ExpectBucketCount(
      "OSCrypt.EncryptorKeyCount.PermanentlyUnavailable", 0, 1);
}

TEST_F(OSCryptAsyncTestWithOSCrypt, FailingKeyProvider) {
  base::HistogramTester histograms;
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<FailingKeyProvider>(
          KeyProvider::KeyError::kPermanentlyUnavailable, "BLAH"));
  OSCryptAsync factory(std::move(providers));
  // TODO: Work out how best to handle provider failures.
  Encryptor encryptor = GetInstanceSync(factory);

  {
    // Encryption should still work, because an empty Encryptor is made which
    // falls back to OSCrypt.
    auto ciphertext = encryptor.EncryptString("secrets");
    EXPECT_TRUE(ciphertext);
    std::string plaintext;
    EXPECT_TRUE(OSCrypt::DecryptString(
        std::string(ciphertext->cbegin(), ciphertext->cend()), &plaintext));
    EXPECT_EQ("secrets", plaintext);
  }
  {
    std::string ciphertext;
    EXPECT_TRUE(OSCrypt::EncryptString("secrets", &ciphertext));
    std::string plaintext;
    // Decryption falls back to OSCrypt if there are no matching providers. In
    // this case, there are no providers at all.
    EXPECT_TRUE(encryptor.DecryptString(ciphertext, &plaintext));
    EXPECT_EQ("secrets", plaintext);
  }

  // Permanently failing key providers never get emplaced into the keyring at
  // all.
  histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount", 1, 1);
  histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount.Available", 0, 1);
  histograms.ExpectBucketCount(
      "OSCrypt.EncryptorKeyCount.TemporarilyUnavailable", 0, 1);
  histograms.ExpectBucketCount(
      "OSCrypt.EncryptorKeyCount.PermanentlyUnavailable", 1, 1);
}

TEST_F(OSCryptAsyncTestWithOSCrypt, TemporarilyFailingKeyProvider) {
  std::optional<std::vector<uint8_t>> ciphertext;

  // First, encrypt some data with the BLAH key provider.
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<TestKeyProvider>("BLAH", /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    ciphertext = encryptor.EncryptString("secrets");
    EXPECT_TRUE(ciphertext);
  }

  // Next, cause this key provider to fail temporarily. This should cause
  // decryption to fail but with kFailureKeyTemporarilyUnavailable.
  {
    base::HistogramTester histograms;
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<FailingKeyProvider>(
            KeyProvider::KeyError::kTemporarilyUnavailable, "BLAH"));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    const auto plaintext = encryptor.DecryptData(*ciphertext, &flags);
    EXPECT_FALSE(plaintext);
    EXPECT_TRUE(flags.temporarily_unavailable);

    // Encryption should still work, even with a temporarily failing key
    // provider, but it will delegate to OSCrypt.
    {
      const auto ciphertext2 = encryptor.EncryptString("secret");
      EXPECT_TRUE(ciphertext2);
      std::string plaintext2;
      EXPECT_TRUE(OSCrypt::DecryptString(
          std::string(ciphertext2->begin(), ciphertext2->end()), &plaintext2));
      EXPECT_EQ(plaintext2, "secret");
    }
    histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount", 1, 1);
    histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount.Available", 0, 1);
    histograms.ExpectBucketCount(
        "OSCrypt.EncryptorKeyCount.TemporarilyUnavailable", 1, 1);
    histograms.ExpectBucketCount(
        "OSCrypt.EncryptorKeyCount.PermanentlyUnavailable", 0, 1);
  }

  // Test permanently unavailable.
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<FailingKeyProvider>(
            KeyProvider::KeyError::kPermanentlyUnavailable, "BLAH"));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    const auto plaintext = encryptor.DecryptData(*ciphertext, &flags);
    // Since there is no key at all, this case has fallback to OSCrypt sync
    // which cannot decrypt data encrypted with BLAH key.
    EXPECT_FALSE(plaintext);
    EXPECT_FALSE(flags.temporarily_unavailable);

    // With no key provided at all (a permanent failure), encryption is
    // delegated to OSCrypt.
    {
      const auto ciphertext2 = encryptor.EncryptString("secret");
      EXPECT_TRUE(ciphertext2);
      std::string plaintext2;
      EXPECT_TRUE(OSCrypt::DecryptString(
          std::string(ciphertext2->begin(), ciphertext2->end()), &plaintext2));
      EXPECT_EQ(plaintext2, "secret");
    }
  }
}

TEST_F(OSCryptAsyncTest, MultipleKeysSomeTemporarilyUnavailable) {
  std::optional<std::vector<uint8_t>> ciphertext;
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<TestKeyProvider>("BLAH", /*use_for_encryption=*/true));
    // Note: TEST is higher precedence so would normally be picked for
    // encryption, were it not unavailable.
    providers.emplace_back(
        /*precedence=*/15u,
        std::make_unique<FailingKeyProvider>(
            KeyProvider::KeyError::kTemporarilyUnavailable, "TEST"));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    ciphertext = encryptor.EncryptString("secret data");
    EXPECT_TRUE(ciphertext);
  }

  // Verify that BLAH is used by creating a new encryptor with only BLAH and
  // decrypting.
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/10u,
        std::make_unique<TestKeyProvider>("BLAH", /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    const auto plaintext = encryptor.DecryptData(*ciphertext);
    EXPECT_TRUE(plaintext);
    EXPECT_EQ(*plaintext, "secret data");
  }
}

TEST_F(OSCryptAsyncTest, ShouldReencrypt) {
  std::string ciphertext;
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("BAR", /*use_for_encryption=*/true));
    providers.emplace_back(
        /*precedence=*/8u,
        std::make_unique<TestKeyProvider>("FOO", /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    ASSERT_TRUE(encryptor.EncryptString("secrets", &ciphertext));
    // FOO should be used, as it's the higher precedence.
    EXPECT_THAT(base::span(ciphertext).first<3>(),
                ::testing::ElementsAreArray(base::span_from_cstring("FOO")));
    std::string plaintext;
    Encryptor::DecryptFlags flags;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_EQ(plaintext, "secrets");
    EXPECT_FALSE(flags.should_reencrypt);
  }

  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("FOO", /*use_for_encryption=*/true));
    providers.emplace_back(
        /*precedence=*/8u,
        std::make_unique<TestKeyProvider>("BAR", /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_EQ(plaintext, "secrets");
    EXPECT_TRUE(flags.should_reencrypt);
  }
}

TEST_F(OSCryptAsyncTestWithOSCrypt, OSCryptShouldReencrypt) {
  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString("secrets", &ciphertext));

  {
    ProviderList providers;
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    std::string plaintext;
    Encryptor::DecryptFlags flags;
    // This encryptor has no providers, so falls back to OSCrypt Sync.
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_EQ(plaintext, "secrets");
    EXPECT_FALSE(flags.should_reencrypt);
  }
  {
    ProviderList providers;
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("FOO", /*use_for_encryption=*/true));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_EQ(plaintext, "secrets");
    EXPECT_TRUE(flags.should_reencrypt);
  }
  {
    ProviderList providers;
    // Specify not to encrypt data with this provider.
    providers.emplace_back(
        /*precedence=*/5u,
        std::make_unique<TestKeyProvider>("FOO", /*use_for_encryption=*/false));
    OSCryptAsync factory(std::move(providers));
    Encryptor encryptor = GetInstanceSync(factory);
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_EQ(plaintext, "secrets");
    EXPECT_FALSE(flags.should_reencrypt);
  }
}

// Test also that if no key providers have OSCrypt sync compatibility then
// encryption simply falls back to OSCrypt, and that OSCrypt can decrypt it
// fine.
TEST_F(OSCryptAsyncTestWithOSCrypt, EncryptorOption) {
  ProviderList providers;
  providers.emplace_back(/*precedence=*/5u,
                         std::make_unique<TestKeyProvider>(
                             "BAR", /*use_for_encryption=*/true,
                             /*compatible_with_os_crypt_sync=*/false));
  OSCryptAsync factory(std::move(providers));
  Encryptor encryptor =
      GetInstanceSync(factory, Encryptor::Option::kEncryptSyncCompat);
  const auto ciphertext = encryptor.EncryptString("os_crypt_secrets");
  ASSERT_TRUE(ciphertext);
  std::string plaintext;
  ASSERT_TRUE(OSCrypt::DecryptString(
      std::string(ciphertext->begin(), ciphertext->end()), &plaintext));
  EXPECT_EQ("os_crypt_secrets", plaintext);
}

using OSCryptAsyncDeathTest = OSCryptAsyncTest;

TEST_F(OSCryptAsyncDeathTest, SamePrecedence) {
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/5u,
      std::make_unique<TestKeyProvider>("A", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/20u,
      std::make_unique<TestKeyProvider>("B", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/5u,
      std::make_unique<TestKeyProvider>("C", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<TestKeyProvider>("D", /*use_for_encryption=*/true));
  EXPECT_DCHECK_DEATH_WITH({ OSCryptAsync factory(std::move(providers)); },
                           "Cannot have two providers with same precedence.");
}

TEST_F(OSCryptAsyncDeathTest, SameName) {
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/5u,
      std::make_unique<TestKeyProvider>("TEST", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<TestKeyProvider>("TEST", /*use_for_encryption=*/true));
  EXPECT_DCHECK_DEATH_WITH(
      {
        OSCryptAsync factory(std::move(providers));
        std::ignore = GetInstanceSync(factory);
      },
      "Tags must not overlap.");
}

TEST_F(OSCryptAsyncDeathTest, OverlappingNames) {
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/5u,
      std::make_unique<TestKeyProvider>("TEST", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<TestKeyProvider>("TEST2", /*use_for_encryption=*/true));
  EXPECT_DCHECK_DEATH_WITH(
      {
        OSCryptAsync factory(std::move(providers));
        std::ignore = GetInstanceSync(factory);
      },
      "Tags must not overlap.");
}

TEST_F(OSCryptAsyncDeathTest, OverlappingNamesBackwards) {
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/5u,
      std::make_unique<TestKeyProvider>("TEST2", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<TestKeyProvider>("TEST", /*use_for_encryption=*/true));
  EXPECT_DCHECK_DEATH_WITH(
      {
        OSCryptAsync factory(std::move(providers));
        std::ignore = GetInstanceSync(factory);
      },
      "Tags must not overlap.");
}

TEST_F(OSCryptAsyncDeathTest, EmptyProviderName) {
  ProviderList providers;
  providers.emplace_back(/*precedence=*/10u,
                         std::make_unique<TestKeyProvider>(
                             std::string(), /*use_for_encryption=*/true));
  EXPECT_DCHECK_DEATH_WITH(
      {
        OSCryptAsync factory(std::move(providers));
        std::ignore = GetInstanceSync(factory);
      },
      "Tag cannot be empty.");
}

TEST_F(OSCryptAsyncTest, NoCrashWithLongNames) {
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<TestKeyProvider>("ABC", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/5u,
      std::make_unique<TestKeyProvider>(
          "TEST_REALLY_LOOOOOOOOOOOOOOOOOOOOOOOOOOOOONG_NAME",
          /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/15u,
      std::make_unique<TestKeyProvider>("XYZ", /*use_for_encryption=*/true));
  OSCryptAsync factory(std::move(providers));
  GetInstanceSync(factory);
}

TEST_F(OSCryptAsyncTest, Metrics) {
  base::HistogramTester histograms;
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<TestKeyProvider>("ABC", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/15u,
      std::make_unique<TestKeyProvider>("DEF", /*use_for_encryption=*/true));
  providers.emplace_back(
      /*precedence=*/20u,
      std::make_unique<FailingKeyProvider>(
          KeyProvider::KeyError::kPermanentlyUnavailable, "GHI"));
  providers.emplace_back(
      /*precedence=*/25u,
      std::make_unique<FailingKeyProvider>(
          KeyProvider::KeyError::kTemporarilyUnavailable, "JKL"));

  OSCryptAsync factory(std::move(providers));
  GetInstanceSync(factory);
  // See TemporarilyFailingKeyProvider, FailingKeyProvider and Empty tests above
  // for further testing of these counts.
  histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount", 4, 1);
  histograms.ExpectBucketCount("OSCrypt.EncryptorKeyCount.Available", 2, 1);
  histograms.ExpectBucketCount(
      "OSCrypt.EncryptorKeyCount.TemporarilyUnavailable", 1, 1);
  histograms.ExpectBucketCount(
      "OSCrypt.EncryptorKeyCount.PermanentlyUnavailable", 1, 1);
}

}  // namespace os_crypt_async
