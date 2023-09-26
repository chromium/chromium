// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager_builder.h"

#include <limits>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/public/identity_manager/identity_test_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/mock_account_manager_facade.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "components/signin/public/identity_manager/ios/fake_device_accounts_provider.h"
#endif

namespace signin {

class MockAccountCapabilitiesFetcherFactory
    : public AccountCapabilitiesFetcherFactory {
 public:
  MockAccountCapabilitiesFetcherFactory() = default;
  ~MockAccountCapabilitiesFetcherFactory() override = default;

  MOCK_METHOD3(
      CreateAccountCapabilitiesFetcher,
      std::unique_ptr<AccountCapabilitiesFetcher>(
          const CoreAccountInfo& account_info,
          AccountCapabilitiesFetcher::FetchPriority fetch_priority,
          AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback));
};

class IdentityManagerBuilderTest : public testing::Test {
 protected:
  IdentityManagerBuilderTest()
      : signin_client_(&pref_service_, &test_url_loader_factory_) {
    IdentityManager::RegisterProfilePrefs(pref_service_.registry());
  }

  ~IdentityManagerBuilderTest() override { signin_client_.Shutdown(); }

  TestSigninClient* GetSigninClient() { return &signin_client_; }

  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return &pref_service_;
  }

 public:
  IdentityManagerBuilderTest(const IdentityManagerBuilderTest&) = delete;
  IdentityManagerBuilderTest& operator=(const IdentityManagerBuilderTest&) =
      delete;

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestSigninClient signin_client_;
};

// Test that IdentityManagerBuilder properly set all required parameters to the
// IdentityManager::InitParameters struct
TEST_F(IdentityManagerBuilderTest, BuildIdentityManagerInitParameters) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath dest_path = temp_dir.GetPath();

#if BUILDFLAG(IS_ANDROID)
  SetUpMockAccountManagerFacade();
#endif

  IdentityManagerBuildParams params;
  params.account_consistency = AccountConsistencyMethod::kDisabled;
  params.image_decoder = std::make_unique<image_fetcher::FakeImageDecoder>();
  params.local_state = GetPrefService();
  params.network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  params.pref_service = GetPrefService();
  params.profile_path = dest_path;
  params.signin_client = GetSigninClient();

#if BUILDFLAG(IS_IOS)
  params.device_accounts_provider =
      std::make_unique<FakeDeviceAccountsProvider>();
  params.account_capabilities_fetcher_factory =
      std::make_unique<MockAccountCapabilitiesFetcherFactory>();
#endif

#if BUILDFLAG(IS_CHROMEOS)
  auto account_manager_facade =
      std::make_unique<account_manager::MockAccountManagerFacade>();
  params.account_manager_facade = account_manager_facade.get();
  params.is_regular_profile = true;
#endif

  const IdentityManager::InitParameters init_params =
      signin::BuildIdentityManagerInitParameters(&params);

  EXPECT_NE(init_params.account_tracker_service, nullptr);
  EXPECT_NE(init_params.token_service, nullptr);
  EXPECT_NE(init_params.gaia_cookie_manager_service, nullptr);
  EXPECT_NE(init_params.primary_account_manager, nullptr);
  EXPECT_NE(init_params.account_fetcher_service, nullptr);
  EXPECT_NE(init_params.primary_account_mutator, nullptr);
  EXPECT_NE(init_params.accounts_cookie_mutator, nullptr);
  EXPECT_NE(init_params.diagnostics_provider, nullptr);
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  EXPECT_NE(init_params.device_accounts_synchronizer, nullptr);
  EXPECT_EQ(init_params.accounts_mutator, nullptr);
#else
  EXPECT_EQ(init_params.device_accounts_synchronizer, nullptr);
  EXPECT_NE(init_params.accounts_mutator, nullptr);
#endif
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_NE(init_params.account_manager_facade, nullptr);
#endif
}

}  // namespace signin
