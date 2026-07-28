// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/private_verification_tokens_installer_policy.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace component_updater {

class PrivateVerificationTokensInstallerPolicyTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  }

 protected:
  base::test::TaskEnvironment env_;
  base::ScopedTempDir component_install_dir_;
};

TEST_F(PrivateVerificationTokensInstallerPolicyTest, VerifyInstallation) {
  std::unique_ptr<ComponentInstallerPolicy> policy =
      std::make_unique<PrivateVerificationTokensInstallerPolicy>(
          base::NullCallback());

  EXPECT_FALSE(policy->VerifyInstallation(base::DictValue(),
                                          component_install_dir_.GetPath()));

  base::FilePath file_path =
      component_install_dir_.GetPath().Append(kPvtConfigFileName);
  ASSERT_TRUE(base::WriteFile(file_path, "{}"));
  EXPECT_TRUE(policy->VerifyInstallation(base::DictValue(),
                                         component_install_dir_.GetPath()));
}

TEST_F(PrivateVerificationTokensInstallerPolicyTest, SimpleGetters) {
  std::unique_ptr<ComponentInstallerPolicy> policy =
      std::make_unique<PrivateVerificationTokensInstallerPolicy>(
          base::NullCallback());

  EXPECT_EQ(policy->GetName(), "Private Verification Token");
  EXPECT_EQ(policy->GetRelativeInstallDir(),
            base::FilePath(FILE_PATH_LITERAL("PrivateVerificationTokens")));
  EXPECT_TRUE(policy->SupportsGroupPolicyEnabledComponentUpdates());
  EXPECT_FALSE(policy->RequiresNetworkEncryption());

  std::vector<uint8_t> hash;
  policy->GetHash(&hash);
  EXPECT_FALSE(hash.empty());
}

TEST_F(PrivateVerificationTokensInstallerPolicyTest, CustomInstall) {
  std::unique_ptr<ComponentInstallerPolicy> policy =
      std::make_unique<PrivateVerificationTokensInstallerPolicy>(
          base::NullCallback());

  // Just verify it doesn't crash, as we don't have easy access to Result
  // members.
  policy->OnCustomInstall(base::DictValue(), base::FilePath());
  SUCCEED();
}

TEST_F(PrivateVerificationTokensInstallerPolicyTest, CustomUninstall) {
  std::unique_ptr<ComponentInstallerPolicy> policy =
      std::make_unique<PrivateVerificationTokensInstallerPolicy>(
          base::NullCallback());

  policy->OnCustomUninstall();
  // If we reached here without crashing, it's a pass.
  SUCCEED();
}

TEST_F(PrivateVerificationTokensInstallerPolicyTest, ParsesValidJson) {
  base::RunLoop run_loop;
  std::string json_content = R"(
    {
      "1": {
        "issuers": [
          {
            "origin": "https://a.example",
            "version": 1,
            "publicKey": "cHZ0LWtleQ==",
            "batchSize": 4,
            "expiration": "12",
            "redeemers": ["https://s1.a.example", "https://s2.a.example"]
          },
          {
            "origin": "https://b.example",
            "version": 1,
            "publicKey": "YW5vdGhlci1hd2Vzb21lLWtleQ==",
            "batchSize": 3,
            "expiration": "24",
            "redeemers": ["https://sub1.b.example", "https://sub2.b.example"]
          }
        ]
      }
    }
  )";

  bool callback_called = false;
  auto callback =
      [&](std::unique_ptr<
          private_verification_tokens::PrivateVerificationTokensIssuerConfig>
              got) {
        callback_called = true;
        ASSERT_TRUE(got);
        EXPECT_THAT(got->config(), testing::SizeIs(2));

        std::string decoded_key_a;
        ASSERT_TRUE(base::Base64Decode("cHZ0LWtleQ==", &decoded_key_a));
        std::vector<uint8_t> expected_key_bytes_a(decoded_key_a.begin(),
                                                  decoded_key_a.end());
        const url::Origin origin_a =
            url::Origin::Create(GURL("https://a.example"));
        const private_verification_tokens::PrivateVerificationTokensPublicKey
            expected_pk_a{origin_a, expected_key_bytes_a,
                          base::Time::UnixEpoch() + base::Seconds(12), 1};

        EXPECT_TRUE(got->config().contains(origin_a));
        const private_verification_tokens::IssuerConfig& config_a =
            got->config().at(origin_a);
        EXPECT_EQ(config_a.batch_size, 4);
        EXPECT_EQ(config_a.public_key, expected_pk_a);
        EXPECT_THAT(config_a.redeemers,
                    testing::ElementsAre(
                        url::Origin::Create(GURL("https://s1.a.example")),
                        url::Origin::Create(GURL("https://s2.a.example"))));

        std::string decoded_key_b;
        ASSERT_TRUE(
            base::Base64Decode("YW5vdGhlci1hd2Vzb21lLWtleQ==", &decoded_key_b));
        std::vector<uint8_t> expected_key_bytes_b(decoded_key_b.begin(),
                                                  decoded_key_b.end());
        const url::Origin origin_b =
            url::Origin::Create(GURL("https://b.example"));
        private_verification_tokens::PrivateVerificationTokensPublicKey
            expected_pk_b{origin_b, expected_key_bytes_b,
                          base::Time::UnixEpoch() + base::Seconds(24), 1};

        EXPECT_TRUE(got->config().contains(origin_b));
        const private_verification_tokens::IssuerConfig& config_b =
            got->config().at(origin_b);
        EXPECT_EQ(config_b.batch_size, 3);
        EXPECT_EQ(config_b.public_key, expected_pk_b);
        EXPECT_THAT(config_b.redeemers,
                    testing::ElementsAre(
                        url::Origin::Create(GURL("https://sub1.b.example")),
                        url::Origin::Create(GURL("https://sub2.b.example"))));

        run_loop.Quit();
      };

  auto policy = std::make_unique<PrivateVerificationTokensInstallerPolicy>(
      base::BindLambdaForTesting(callback));

  base::FilePath file_path =
      component_install_dir_.GetPath().Append(kPvtConfigFileName);

  ASSERT_TRUE(base::WriteFile(file_path, json_content));

  policy->ComponentReadyForTesting(
      base::Version(), component_install_dir_.GetPath(), base::DictValue());

  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(PrivateVerificationTokensInstallerPolicyTest, IgnoresInvalidJson) {
  base::RunLoop run_loop;
  bool callback_called = false;
  auto callback =
      [&](std::unique_ptr<
          private_verification_tokens::PrivateVerificationTokensIssuerConfig>
              got) {
        callback_called = true;
        EXPECT_FALSE(got);
        run_loop.Quit();
      };

  auto policy = std::make_unique<PrivateVerificationTokensInstallerPolicy>(
      base::BindLambdaForTesting(callback));

  base::FilePath file_path =
      component_install_dir_.GetPath().Append(kPvtConfigFileName);

  ASSERT_TRUE(base::WriteFile(file_path, "invalid json"));

  policy->ComponentReadyForTesting(
      base::Version(), component_install_dir_.GetPath(), base::DictValue());

  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

}  // namespace component_updater
