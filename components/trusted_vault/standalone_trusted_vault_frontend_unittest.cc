// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_frontend.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/local_domains_storage.h"
#include "components/trusted_vault/test/fake_local_domains_storage_file_access.h"
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

constexpr char kTestEmail[] = "test@gmail.com";

#if BUILDFLAG(IS_MAC)
constexpr char kKeychainAccessGroupPrefix[] = "test-access-group-prefix";
constexpr char kKeychainAccessGroup[] =
    "test-access-group-prefix.com.google.common.folsom";
#endif

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
  base::test::ScopedFeatureList scoped_feature_list_{
      kTrustedVaultSharedRecoveryFactorsAndEncryption};
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

  // Issue StoreKeys and FetchKeys before the backend is initialized.
  frontend->StoreKeys(account_info.gaia, SecurityDomainId::kChromeSync, {kKey},
                      /*last_key_version=*/1);
  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&>
      fetch_keys_future;
  frontend->FetchKeys(account_info, SecurityDomainId::kChromeSync,
                      fetch_keys_future.GetCallback());

  task_environment_.RunUntilIdle();
  // FetchKeys must not complete before InitializeBackendForTesting() is called.
  EXPECT_FALSE(fetch_keys_future.IsReady());

  // Initialize the backend; ReadDataFromDisk and the queued StoreKeys and
  // FetchKeys tasks should now run in FIFO order.
  frontend->InitializeBackendForTesting(LocalDomainsStorage::CreateForTesting(
      std::make_unique<FakeLocalDomainsStorageFileAccess>()));
  EXPECT_THAT(fetch_keys_future.Get(), ElementsAre(kKey));
}

}  // namespace
}  // namespace trusted_vault
