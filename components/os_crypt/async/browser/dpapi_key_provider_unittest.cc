// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/dpapi_key_provider.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/random.h"
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
  Encryptor GetInstanceWithDPAPI() {
    std::vector<std::pair<size_t, std::unique_ptr<KeyProvider>>> providers;
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u, std::make_unique<DPAPIKeyProvider>(&prefs_)));
    OSCryptAsync factory(std::move(providers));
    base::RunLoop run_loop;
    std::optional<Encryptor> encryptor;
    auto sub = factory.GetInstance(base::BindLambdaForTesting(
        [&](Encryptor encryptor_param, bool success) {
          EXPECT_TRUE(success);
          encryptor.emplace(std::move(encryptor_param));
          run_loop.Quit();
        }));
    run_loop.Run();
    return std::move(*encryptor);
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
