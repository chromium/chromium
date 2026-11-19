// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/local_gpm_passkeys_util.h"

#include <memory>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/trusted_vault/features.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/public/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::StrictMock;

class LocalGpmPasskeysUtilTest : public testing::Test {
 protected:
  std::unique_ptr<TestingProfile> CreateProfileWithMockEnclave(
      StrictMock<MockEnclaveManager>** out_mock) {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        EnclaveManagerFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<StrictMock<MockEnclaveManager>>();
            }));
    std::unique_ptr<TestingProfile> profile = builder.Build();
    *out_mock = static_cast<StrictMock<MockEnclaveManager>*>(
        EnclaveManagerFactory::GetForProfile(profile.get()));
    return profile;
  }

  std::unique_ptr<TestingProfile> CreateProfileExpectingNoEnclaveLookup() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        EnclaveManagerFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              ADD_FAILURE() << "EnclaveManager should not be queried";
              return nullptr;
            }));
    return builder.Build();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LocalGpmPasskeysUtilTest, IsLocalGpmPasskeysEnabledRequiresBothFlags) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            device::kWebAuthnLocalGpmPasskeys,
            trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption});
    EXPECT_FALSE(IsLocalGpmPasskeysEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{device::kWebAuthnLocalGpmPasskeys},
        /*disabled_features=*/{
            trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption});
    EXPECT_FALSE(IsLocalGpmPasskeysEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/
        {trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption},
        /*disabled_features=*/{device::kWebAuthnLocalGpmPasskeys});
    EXPECT_FALSE(IsLocalGpmPasskeysEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/
        {device::kWebAuthnLocalGpmPasskeys,
         trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption},
        /*disabled_features=*/{});
    EXPECT_TRUE(IsLocalGpmPasskeysEnabled());
  }
}

TEST_F(LocalGpmPasskeysUtilTest, ShouldUseLocalGpmPasskeysWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{device::kWebAuthnLocalGpmPasskeys},
      /*disabled_features=*/{
          trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption});

  std::unique_ptr<TestingProfile> profile =
      CreateProfileExpectingNoEnclaveLookup();

  base::test::TestFuture<bool> future;
  ShouldUseLocalGpmPasskeys(profile.get(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(LocalGpmPasskeysUtilTest, ShouldUseLocalGpmPasskeysAllUsersMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{device::kWebAuthnLocalGpmPasskeys, {{"mode", "all_users"}}},
       {trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption, {}}},
      /*disabled_features=*/{});

  std::unique_ptr<TestingProfile> profile =
      CreateProfileExpectingNoEnclaveLookup();

  base::test::TestFuture<bool> future;
  ShouldUseLocalGpmPasskeys(profile.get(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(LocalGpmPasskeysUtilTest,
       ShouldUseLocalGpmPasskeysNewUsersOnlyUnregistered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{device::kWebAuthnLocalGpmPasskeys, {{"mode", "new_users_only"}}},
       {trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption, {}}},
      /*disabled_features=*/{});

  StrictMock<MockEnclaveManager>* mock_enclave = nullptr;
  std::unique_ptr<TestingProfile> profile =
      CreateProfileWithMockEnclave(&mock_enclave);
  ASSERT_TRUE(mock_enclave);

  EXPECT_CALL(*mock_enclave, LoadAndCheckIsRegistered)
      .WillOnce(RunOnceCallback<0>(false));

  base::test::TestFuture<bool> future;
  ShouldUseLocalGpmPasskeys(profile.get(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(LocalGpmPasskeysUtilTest,
       ShouldUseLocalGpmPasskeysNewUsersOnlyRegistered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{device::kWebAuthnLocalGpmPasskeys, {{"mode", "new_users_only"}}},
       {trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption, {}}},
      /*disabled_features=*/{});

  StrictMock<MockEnclaveManager>* mock_enclave = nullptr;
  std::unique_ptr<TestingProfile> profile =
      CreateProfileWithMockEnclave(&mock_enclave);
  ASSERT_TRUE(mock_enclave);

  EXPECT_CALL(*mock_enclave, LoadAndCheckIsRegistered)
      .WillOnce(RunOnceCallback<0>(true));

  base::test::TestFuture<bool> future;
  ShouldUseLocalGpmPasskeys(profile.get(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(LocalGpmPasskeysUtilTest,
       ShouldUseLocalGpmPasskeysNewUsersOnlyNullEnclaveManager) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{device::kWebAuthnLocalGpmPasskeys, {{"mode", "new_users_only"}}},
       {trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption, {}}},
      /*disabled_features=*/{});

  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      EnclaveManagerFactory::GetInstance(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));
  std::unique_ptr<TestingProfile> profile = builder.Build();

  base::test::TestFuture<bool> future;
  ShouldUseLocalGpmPasskeys(profile.get(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace
}  // namespace webauthn
