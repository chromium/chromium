// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_frontend.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/local_domains_storage.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#include "components/trusted_vault/test/fake_local_domains_storage_file_access.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {
namespace {

using testing::ElementsAre;
using testing::Eq;

constexpr char kTestEmail[] = "test@gmail.com";

#if BUILDFLAG(IS_MAC)
constexpr char kKeychainAccessGroupPrefix[] = "test-access-group-prefix";
constexpr char kKeychainAccessGroup[] =
    "test-access-group-prefix.com.google.common.folsom";
#endif

class MockTrustedVaultClientObserver : public TrustedVaultClient::Observer {
 public:
  MockTrustedVaultClientObserver() = default;
  ~MockTrustedVaultClientObserver() override = default;

  MOCK_METHOD(void,
              OnTrustedVaultKeysChanged,
              (std::optional<TrustedVaultUserActionTriggerForUMA>),
              (override));
  MOCK_METHOD(void, OnTrustedVaultRecoverabilityChanged, (), (override));
};

class StandaloneTrustedVaultFrontendTest : public testing::Test {
 public:
  StandaloneTrustedVaultFrontendTest() = default;
  ~StandaloneTrustedVaultFrontendTest() override = default;

  void TearDown() override {
    // Ensure pending backend deletions run on `task_environment_`.
    task_environment_.RunUntilIdle();
  }

 protected:
#if BUILDFLAG(IS_MAC)
  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{kKeychainAccessGroup};
#endif
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(StandaloneTrustedVaultFrontendTest,
       ShouldQueueTasksUntilBackendInitialized) {
  const std::vector<uint8_t> kKey = {1, 2, 3, 4};
  const CoreAccountInfo account_info =
      identity_test_env_
          .MakePrimaryAccountAvailable(kTestEmail,
                                       signin::ConsentLevel::kSignin)
          .GetCoreAccountInfo();

  scoped_refptr<StandaloneTrustedVaultFrontend> frontend =
      StandaloneTrustedVaultFrontend::CreateForTesting(
#if BUILDFLAG(IS_MAC)
          kKeychainAccessGroupPrefix,
#endif
          identity_test_env_.identity_manager(),
          test_url_loader_factory_.GetSafeWeakWrapper());

  StandaloneTrustedVaultClient client(SecurityDomainId::kChromeSync, frontend);
  testing::StrictMock<MockTrustedVaultClientObserver> observer;
  base::ScopedObservation<TrustedVaultClient, TrustedVaultClient::Observer>
      observation(&observer);
  observation.Observe(&client);

  // Issue `StoreKeys()`, `FetchKeys()`, and `CanEnroll()` before the backend is
  // initialized.
  EXPECT_CALL(observer, OnTrustedVaultKeysChanged).Times(0);
  frontend->StoreKeys(account_info.gaia, SecurityDomainId::kChromeSync, {kKey},
                      /*last_key_version=*/1,
                      TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&>
      fetch_keys_future;
  frontend->FetchKeys(account_info, SecurityDomainId::kChromeSync,
                      fetch_keys_future.GetCallback());
  base::test::TestFuture<bool> can_enroll_future;
  frontend->CanEnroll(account_info, SecurityDomainId::kChromeSync,
                      can_enroll_future.GetCallback());

  task_environment_.RunUntilIdle();
  // `FetchKeys()` and `CanEnroll()` must not complete before
  // `InitializeBackendForTesting()` is called.
  EXPECT_FALSE(fetch_keys_future.IsReady());
  EXPECT_FALSE(can_enroll_future.IsReady());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Initialize the backend; `ReadDataFromDisk()` and the queued `StoreKeys()`,
  // `FetchKeys()`, and `CanEnroll()` tasks should now run in FIFO order, and
  // the queued trigger should be propagated to observers.
  EXPECT_CALL(observer,
              OnTrustedVaultKeysChanged(
                  Eq(TrustedVaultUserActionTriggerForUMA::kProfileMenu)));
  frontend->InitializeBackendForTesting(LocalDomainsStorage::CreateForTesting(
      std::make_unique<FakeLocalDomainsStorageFileAccess>()));
  EXPECT_THAT(fetch_keys_future.Get(), ElementsAre(kKey));
  EXPECT_FALSE(can_enroll_future.Get());
}

TEST_F(StandaloneTrustedVaultFrontendTest,
       ShouldInitializeBackendAndExecutePendingTasksOnceOSCryptAsyncIsReady) {
  base::test::ScopedFeatureList scoped_feature_list{
      kTrustedVaultSharedRecoveryFactorsAndEncryption};
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      os_crypt_async::GetTestOSCryptAsyncForTesting();
  const std::vector<uint8_t> kKey = {5, 6, 7, 8};
  const CoreAccountInfo account_info =
      identity_test_env_
          .MakePrimaryAccountAvailable(kTestEmail,
                                       signin::ConsentLevel::kSignin)
          .GetCoreAccountInfo();

  scoped_refptr<StandaloneTrustedVaultFrontend> frontend =
      base::MakeRefCounted<StandaloneTrustedVaultFrontend>(
          os_crypt_async.get(),
#if BUILDFLAG(IS_MAC)
          kKeychainAccessGroupPrefix,
#endif
          temp_dir.GetPath(), identity_test_env_.identity_manager(),
          test_url_loader_factory_.GetSafeWeakWrapper());

  frontend->StoreKeys(account_info.gaia, SecurityDomainId::kChromeSync, {kKey},
                      /*last_key_version=*/2, /*trigger=*/std::nullopt);
  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&>
      fetch_keys_future;
  frontend->FetchKeys(account_info, SecurityDomainId::kChromeSync,
                      fetch_keys_future.GetCallback());

  EXPECT_THAT(fetch_keys_future.Get(), ElementsAre(kKey));
}

TEST_F(StandaloneTrustedVaultFrontendTest,
       ShouldInitializeBackendWhenFeatureDisabledAndOSCryptIsNull) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kTrustedVaultSharedRecoveryFactorsAndEncryption);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const std::vector<uint8_t> kKey = {5, 6, 7, 8};
  const CoreAccountInfo account_info =
      identity_test_env_
          .MakePrimaryAccountAvailable(kTestEmail,
                                       signin::ConsentLevel::kSignin)
          .GetCoreAccountInfo();

  scoped_refptr<StandaloneTrustedVaultFrontend> frontend =
      base::MakeRefCounted<StandaloneTrustedVaultFrontend>(
          /*os_crypt_async=*/nullptr,
#if BUILDFLAG(IS_MAC)
          kKeychainAccessGroupPrefix,
#endif
          temp_dir.GetPath(), identity_test_env_.identity_manager(),
          test_url_loader_factory_.GetSafeWeakWrapper());

  frontend->StoreKeys(account_info.gaia, SecurityDomainId::kChromeSync, {kKey},
                      /*last_key_version=*/2, /*trigger=*/std::nullopt);
  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&>
      fetch_keys_future;
  frontend->FetchKeys(account_info, SecurityDomainId::kChromeSync,
                      fetch_keys_future.GetCallback());

  EXPECT_THAT(fetch_keys_future.Get(), ElementsAre(kKey));
}

}  // namespace
}  // namespace trusted_vault
