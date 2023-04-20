// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "crypto/hkdf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace os_crypt_async {

class OSCryptAsyncTest : public ::testing::Test {
 protected:
  using ProviderList =
      std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>>;

  Encryptor GetInstanceSync(OSCryptAsync& factory) {
    base::RunLoop run_loop;
    absl::optional<Encryptor> encryptor;
    auto sub = factory.GetInstance(base::BindLambdaForTesting(
        [&](Encryptor encryptor_param, bool success) {
          EXPECT_TRUE(success);
          encryptor.emplace(std::move(encryptor_param));
          run_loop.Quit();
        }));
    run_loop.Run();
    return std::move(*encryptor);
  }

  base::test::TaskEnvironment task_environment_;
};

class TestKeyProvider : public KeyProvider {
 public:
  explicit TestKeyProvider(const std::string& name = "TEST",
                           bool use_for_encryption = true)
      : name_(name), use_for_encryption_(use_for_encryption) {}

 protected:
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

  const bool use_for_encryption_;
};

TEST_F(OSCryptAsyncTest, EncryptHeader) {
  const std::string kTestProviderName("TEST");
  ProviderList providers;
  providers.emplace_back(std::make_pair(
      10u, std::make_unique<TestKeyProvider>(kTestProviderName)));
  OSCryptAsync factory(std::move(providers));
  Encryptor encryptor = GetInstanceSync(factory);

  auto ciphertext = encryptor.EncryptString("secrets");
  ASSERT_TRUE(std::equal(kTestProviderName.cbegin(), kTestProviderName.cend(),
                         ciphertext->cbegin()));
}

TEST_F(OSCryptAsyncTest, TwoProvidersBothEnabled) {
  absl::optional<std::vector<uint8_t>> ciphertext;
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
}

TEST_F(OSCryptAsyncTest, TwoProvidersOneEnabled) {
  absl::optional<std::vector<uint8_t>> ciphertext;
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

class SlowTestKeyProvider : public TestKeyProvider {
 public:
  explicit SlowTestKeyProvider(base::TimeDelta sleep_time)
      : sleep_time_(sleep_time) {}

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
  std::list<base::CallbackListSubscription> subs;
  for (size_t call = 0; call < kExpectedCalls; call++) {
    subs.push_back(factory.GetInstance(base::BindLambdaForTesting(
        [&calls, &run_loop](Encryptor encryptor, bool success) {
          calls++;
          if (calls == kExpectedCalls) {
            run_loop.Quit();
          }
        })));
  }
  run_loop.Run();
  EXPECT_EQ(calls, kExpectedCalls);
}

// This test verifies that if the subscription from CallbackList moves out of
// scope, then the callback never occurs.
TEST_F(OSCryptAsyncTest, SubscriptionCancelled) {
  ProviderList providers;
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<SlowTestKeyProvider>(base::Seconds(1)));
  OSCryptAsync factory(std::move(providers));

  {
    auto sub = factory.GetInstance(
        base::BindOnce([](Encryptor encryptor, bool success) {
          // This should not be called, as the subscription went out of scope.
          NOTREACHED();
        }));
  }

  // Complete the init on a nested RunLoop.
  base::RunLoop run_loop;
  auto sub = factory.GetInstance(
      base::BindLambdaForTesting([&](Encryptor encryptor_param, bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
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
}

TEST_F(OSCryptAsyncTest, TestEncryptorInterface) {
  auto encryptor = GetTestEncryptorForTesting();
  auto ciphertext = encryptor.EncryptString("testsecrets");
  ASSERT_TRUE(ciphertext);
  auto decrypted = encryptor.DecryptData(*ciphertext);
  ASSERT_TRUE(decrypted);
  EXPECT_EQ(*decrypted, "testsecrets");
}

class FailingKeyProvider : public TestKeyProvider {
 private:
  void GetKey(KeyCallback callback) override {
    std::move(callback).Run("", absl::nullopt);
  }
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
  ProviderList providers;
  OSCryptAsync factory(std::move(providers));
  Encryptor encryptor = GetInstanceSync(factory);
  std::string ciphertext;
  EXPECT_TRUE(OSCrypt::EncryptString("secrets", &ciphertext));
  std::string plaintext;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &plaintext));
  EXPECT_EQ("secrets", plaintext);
}

TEST_F(OSCryptAsyncTestWithOSCrypt, FailingKeyProvider) {
  ProviderList providers;
  providers.emplace_back(/*precedence=*/10u,
                         std::make_unique<FailingKeyProvider>());
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

}  // namespace os_crypt_async
