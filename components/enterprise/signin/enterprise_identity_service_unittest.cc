// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/signin/enterprise_identity_service.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise {

using ::testing::StrictMock;

namespace {

constexpr char kAccessToken1[] = "access_token1";
constexpr char kAccessToken2[] = "access_token2";

class MockEnterpriseIdentityServiceObserver
    : public EnterpriseIdentityService::Observer {
 public:
  MockEnterpriseIdentityServiceObserver() = default;
  ~MockEnterpriseIdentityServiceObserver() override = default;

  MOCK_METHOD(void, OnManagedAccountSessionChanged, (), (override));
};

}  // namespace

class EnterpriseIdentityServiceTest : public testing::Test {
 protected:
  EnterpriseIdentityServiceTest() {
    identity_env_.WaitForRefreshTokensLoaded();
  }

  std::unique_ptr<EnterpriseIdentityService> CreateService() {
    return EnterpriseIdentityService::Create(identity_env_.identity_manager());
  }

  const signin::IdentityManager* identity_manager() const {
    return identity_env_.identity_manager();
  }

  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_env_;
};

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsWithRefreshTokens_NoUser) {
  ASSERT_TRUE(identity_manager()->AreRefreshTokensLoaded());
  ASSERT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  auto service = CreateService();
  ASSERT_TRUE(service);
  base::test::TestFuture<std::vector<CoreAccountInfo>> test_future;
  service->GetManagedAccountsWithRefreshTokens(test_future.GetCallback());

  EXPECT_TRUE(test_future.Get().empty());
}

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsWithRefreshTokens_SingleManagedUser_Async) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");

  auto service = CreateService();
  base::test::TestFuture<std::vector<CoreAccountInfo>> test_future;
  service->GetManagedAccountsWithRefreshTokens(test_future.GetCallback());

  // Value not already available.
  EXPECT_FALSE(test_future.IsReady());

  // Full info becomes available.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  auto managed_accounts = test_future.Get();
  ASSERT_EQ(managed_accounts.size(), 1U);
  EXPECT_EQ(managed_accounts.front(), account);
}

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsWithRefreshTokens_SingleManagedUser_Sync) {
  // google.com accounts are determined as managed accounts synchronously.
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@google.com");

  auto service = CreateService();
  base::test::TestFuture<std::vector<CoreAccountInfo>> test_future;
  service->GetManagedAccountsWithRefreshTokens(test_future.GetCallback());

  // Value is already available.
  EXPECT_TRUE(test_future.IsReady());

  auto managed_accounts = test_future.Get();
  ASSERT_EQ(managed_accounts.size(), 1U);
  EXPECT_EQ(managed_accounts.front(), account);
}

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsWithRefreshTokens_NoRefreshTokens_ThenOneUser) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");
  identity_env_.ResetToAccountsNotYetLoadedFromDiskState();
  EXPECT_FALSE(identity_manager()->AreRefreshTokensLoaded());

  auto service = CreateService();
  base::test::TestFuture<std::vector<CoreAccountInfo>> test_future;
  service->GetManagedAccountsWithRefreshTokens(test_future.GetCallback());

  // Value not already available, as the request is waiting for refresh tokens
  // to have been loaded.
  EXPECT_FALSE(test_future.IsReady());

  identity_env_.ReloadAccountsFromDisk();
  identity_env_.WaitForRefreshTokensLoaded();

  EXPECT_FALSE(test_future.IsReady());

  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  auto managed_accounts = test_future.Get();
  ASSERT_EQ(managed_accounts.size(), 1U);
  EXPECT_EQ(managed_accounts.front(), account);
}

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsWithRefreshTokens_MultipleMixedUsers) {
  AccountInfo google_account =
      identity_env_.MakeAccountAvailable("account@google.com");
  AccountInfo async_enterprise_account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");
  AccountInfo gmail_account =
      identity_env_.MakeAccountAvailable("account@gmail.com");
  AccountInfo async_consumer_account =
      identity_env_.MakeAccountAvailable("account@consumer.com");

  auto service = CreateService();
  base::test::TestFuture<std::vector<CoreAccountInfo>> test_future;
  service->GetManagedAccountsWithRefreshTokens(test_future.GetCallback());

  EXPECT_FALSE(test_future.IsReady());

  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      async_enterprise_account.account_id, async_enterprise_account.email,
      async_enterprise_account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  EXPECT_FALSE(test_future.IsReady());

  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      async_consumer_account.account_id, async_consumer_account.email,
      async_consumer_account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  auto managed_accounts = test_future.Get();
  ASSERT_EQ(managed_accounts.size(), 2U);
  ASSERT_THAT(managed_accounts, testing::UnorderedElementsAre(
                                    google_account, async_enterprise_account));
}

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsAccessTokens_SingleManagedUser_Success) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  auto service = CreateService();
  base::test::TestFuture<std::vector<std::string>> test_future;
  service->GetManagedAccountsAccessTokens(test_future.GetCallback());

  identity_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kAccessToken1, base::Time::Max(), /*id_token=*/std::string(),
          signin::ScopeSet{GaiaConstants::kDeviceManagementServiceOAuth});

  EXPECT_THAT(test_future.Get(), testing::ElementsAre(kAccessToken1));
}

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsAccessTokens_SingleManagedUser_Error) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  auto service = CreateService();
  base::test::TestFuture<std::vector<std::string>> test_future;
  service->GetManagedAccountsAccessTokens(test_future.GetCallback());

  identity_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));

  EXPECT_TRUE(test_future.Get().empty());
}

TEST_F(EnterpriseIdentityServiceTest,
       GetManagedAccountsAccessTokens_MixedUsers) {
  AccountInfo google_account =
      identity_env_.MakeAccountAvailable("account@google.com");
  AccountInfo async_enterprise_account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");
  AccountInfo gmail_account =
      identity_env_.MakeAccountAvailable("account@gmail.com");
  AccountInfo async_consumer_account =
      identity_env_.MakeAccountAvailable("account@consumer.com");

  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      async_enterprise_account.account_id, async_enterprise_account.email,
      async_enterprise_account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      async_consumer_account.account_id, async_consumer_account.email,
      async_consumer_account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  auto service = CreateService();
  base::test::TestFuture<std::vector<std::string>> test_future;
  service->GetManagedAccountsAccessTokens(test_future.GetCallback());

  identity_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      google_account.account_id, kAccessToken1, base::Time::Max());
  identity_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      async_enterprise_account.account_id, kAccessToken2, base::Time::Max());

  EXPECT_THAT(test_future.Get(),
              testing::UnorderedElementsAre(kAccessToken1, kAccessToken2));
}

TEST_F(EnterpriseIdentityServiceTest,
       Observer_OnManagedAccountSessionChanged_SyncManagedUser) {
  StrictMock<MockEnterpriseIdentityServiceObserver> observer;
  EXPECT_CALL(observer, OnManagedAccountSessionChanged).Times(1);

  auto service = CreateService();
  service->AddObserver(&observer);
  identity_env_.MakeAccountAvailable("account@google.com");

  service->RemoveObserver(&observer);
}

TEST_F(EnterpriseIdentityServiceTest,
       Observer_OnManagedAccountSessionChanged_SyncConsumerUser) {
  StrictMock<MockEnterpriseIdentityServiceObserver> observer;
  EXPECT_CALL(observer, OnManagedAccountSessionChanged).Times(0);

  auto service = CreateService();
  service->AddObserver(&observer);
  identity_env_.MakeAccountAvailable("account@gmail.com");

  service->RemoveObserver(&observer);
}

TEST_F(EnterpriseIdentityServiceTest,
       Observer_OnManagedAccountSessionChanged_AsyncManagedUser) {
  base::RunLoop run_loop;
  StrictMock<MockEnterpriseIdentityServiceObserver> observer;
  EXPECT_CALL(observer, OnManagedAccountSessionChanged)
      .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));

  auto service = CreateService();
  service->AddObserver(&observer);
  auto async_enterprise_account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      async_enterprise_account.account_id, async_enterprise_account.email,
      async_enterprise_account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  run_loop.Run();

  service->RemoveObserver(&observer);
}

TEST_F(EnterpriseIdentityServiceTest,
       Observer_OnManagedAccountSessionChanged_ManagedUserRefresh) {
  auto account = identity_env_.MakeAccountAvailable("account@google.com");

  StrictMock<MockEnterpriseIdentityServiceObserver> observer;
  auto service = CreateService();
  service->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnManagedAccountSessionChanged)
      .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
  identity_env_.SetRefreshTokenForAccount(account.account_id);
  run_loop.Run();

  service->RemoveObserver(&observer);
}

}  // namespace enterprise
