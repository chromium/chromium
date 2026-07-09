// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade_impl.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_manager_test_util.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace account_manager {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::WithArgs;

constexpr char kTestAccountEmail[] = "test@gmail.com";
constexpr char kAnotherTestAccountEmail[] = "another_test@gmail.com";
constexpr char kFakeClientId[] = "fake-client-id";
constexpr char kFakeClientSecret[] = "fake-client-secret";
constexpr char kFakeAccessToken[] = "fake-access-token";
constexpr char kFakeIdToken[] = "fake-id-token";

constexpr char kMojoDisconnectionsAccountManagerRemote[] =
    "AccountManager.MojoDisconnections.AccountManagerRemote";
constexpr char kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote[] =
    "AccountManager.MojoDisconnections.AccessTokenFetcherRemote";

void AccessTokenFetchSuccess(
    base::OnceCallback<void(crosapi::mojom::AccessTokenResultPtr)> callback) {
  crosapi::mojom::AccessTokenInfoPtr access_token_info =
      crosapi::mojom::AccessTokenInfo::New(kFakeAccessToken, base::Time::Now(),
                                           kFakeIdToken);
  crosapi::mojom::AccessTokenResultPtr result =
      crosapi::mojom::AccessTokenResult::NewAccessTokenInfo(
          std::move(access_token_info));
  std::move(callback).Run(std::move(result));
}

void AccessTokenFetchServiceError(
    base::OnceCallback<void(crosapi::mojom::AccessTokenResultPtr)> callback) {
  crosapi::mojom::AccessTokenResultPtr result =
      crosapi::mojom::AccessTokenResult::NewError(
          account_manager::ToMojoGoogleServiceAuthError(
              GoogleServiceAuthError::FromServiceError(std::string())));
  std::move(callback).Run(std::move(result));
}

class MockAccessTokenFetcher : public crosapi::mojom::AccessTokenFetcher {
 public:
  MockAccessTokenFetcher() : receiver_(this) {}
  MockAccessTokenFetcher(const MockAccessTokenFetcher&) = delete;
  MockAccessTokenFetcher& operator=(const MockAccessTokenFetcher&) = delete;
  ~MockAccessTokenFetcher() override = default;

  void Bind(
      mojo::PendingReceiver<crosapi::mojom::AccessTokenFetcher> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void ResetReceiver() { receiver_.reset(); }

  // crosapi::mojom::AccessTokenFetcher override.
  MOCK_METHOD(void,
              Start,
              (const std::vector<std::string>& scopes, StartCallback callback),
              (override));

 private:
  mojo::Receiver<crosapi::mojom::AccessTokenFetcher> receiver_;
};

class MockOAuthConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockOAuthConsumer() = default;
  MockOAuthConsumer(const MockOAuthConsumer&) = delete;
  MockOAuthConsumer& operator=(const MockOAuthConsumer&) = delete;
  ~MockOAuthConsumer() override = default;

  // OAuth2AccessTokenConsumer overrides.
  MOCK_METHOD(void,
              OnGetTokenSuccess,
              (const TokenResponse& token_response),
              (override));
  MOCK_METHOD(void,
              OnGetTokenFailure,
              (const GoogleServiceAuthError& error),
              (override));

  std::string GetConsumerName() const override {
    return "account_manager_facade_impl_unittest";
  }
};

class FakeAccountManager : public crosapi::mojom::AccountManager {
 public:
  FakeAccountManager() = default;
  FakeAccountManager(const FakeAccountManager&) = delete;
  FakeAccountManager& operator=(const FakeAccountManager&) = delete;
  ~FakeAccountManager() override = default;

  void SetMockAccessTokenFetcher(
      std::unique_ptr<MockAccessTokenFetcher> mock_access_token_fetcher) {
    access_token_fetcher_ = std::move(mock_access_token_fetcher);
  }

  void CreateAccessTokenFetcher(
      crosapi::mojom::AccountKeyPtr mojo_account_key,
      const std::string& oauth_consumer_name,
      CreateAccessTokenFetcherCallback callback) override {
    if (!access_token_fetcher_)
      access_token_fetcher_ = std::make_unique<MockAccessTokenFetcher>();
    mojo::PendingRemote<crosapi::mojom::AccessTokenFetcher> pending_remote;
    access_token_fetcher_->Bind(
        pending_remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(pending_remote));
  }

  mojo::Remote<crosapi::mojom::AccountManager> CreateRemote() {
    mojo::Remote<crosapi::mojom::AccountManager> remote;
    receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  void SetAccounts(const std::vector<Account>& accounts) {
    accounts_ = accounts;
  }

  void ClearReceivers() { receivers_.Clear(); }

 private:
  std::vector<Account> accounts_;
  std::unique_ptr<MockAccessTokenFetcher> access_token_fetcher_;
  mojo::ReceiverSet<crosapi::mojom::AccountManager> receivers_;
};

MATCHER_P(AccountEq, expected_account, "") {
  return testing::ExplainMatchResult(
             testing::Field(&Account::key, testing::Eq(expected_account.key)),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Field(&Account::raw_email,
                            testing::StrEq(expected_account.raw_email)),
             arg, result_listener);
}

}  // namespace

class AccountManagerFacadeImplTest : public testing::Test {
 public:
  AccountManagerFacadeImplTest() = default;
  AccountManagerFacadeImplTest(const AccountManagerFacadeImplTest&) = delete;
  AccountManagerFacadeImplTest& operator=(const AccountManagerFacadeImplTest&) =
      delete;
  ~AccountManagerFacadeImplTest() override = default;

 protected:
  void SetUp() override {
    AccountManager::RegisterPrefs(pref_service_.registry());
    real_account_manager_ = std::make_unique<AccountManager>();
    real_account_manager_->InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper());
    real_account_manager_->SetPrefService(&pref_service_);
  }

  FakeAccountManager& account_manager() { return account_manager_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  AccountManager* real_account_manager() { return real_account_manager_.get(); }

  std::unique_ptr<AccountManagerFacadeImpl> CreateFacade() {
    base::test::TestFuture<void> future;
    auto result = std::make_unique<AccountManagerFacadeImpl>(
        account_manager().CreateRemote(),
        /*remote_version=*/std::numeric_limits<uint32_t>::max(),
        real_account_manager(), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return result;
  }

  Account AddTestGaiaAccount(const std::string& email,
                             const std::string& token) {
    Account account = CreateTestGaiaAccount(email);
    real_account_manager()->UpsertAccount(account.key, account.raw_email,
                                          token);
    return account;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeAccountManager account_manager_;
  base::HistogramTester histogram_tester_;

  TestingPrefServiceSimple pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<AccountManager> real_account_manager_;
};

TEST_F(AccountManagerFacadeImplTest, OnTokenUpsertedIsPropagatedToObservers) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  testing::StrictMock<MockAccountManagerFacadeObserver> observer;
  base::ScopedObservation<AccountManagerFacade, AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(account_manager_facade.get());

  Account account = CreateTestGaiaAccount(kTestAccountEmail);
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer, OnAccountUpserted(AccountEq(account)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  real_account_manager()->UpsertAccount(account.key, account.raw_email,
                                        "test_token");
  EXPECT_TRUE(future.Wait());
}

TEST_F(AccountManagerFacadeImplTest, OnAccountRemovedIsPropagatedToObservers) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();

  testing::StrictMock<MockAccountManagerFacadeObserver> observer;
  base::ScopedObservation<AccountManagerFacade, AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(account_manager_facade.get());

  Account account = CreateTestGaiaAccount(kTestAccountEmail);

  base::test::TestFuture<void> upsert_future;
  EXPECT_CALL(observer, OnAccountUpserted(AccountEq(account)))
      .WillOnce(base::test::RunOnceClosure(upsert_future.GetCallback()));
  real_account_manager()->UpsertAccount(account.key, account.raw_email,
                                        "test_token");
  EXPECT_TRUE(upsert_future.Wait());

  base::test::TestFuture<void> remove_future;
  EXPECT_CALL(observer, OnAccountRemoved(AccountEq(account)))
      .WillOnce(base::test::RunOnceClosure(remove_future.GetCallback()));
  real_account_manager()->RemoveAccount(account.key);
  EXPECT_TRUE(remove_future.Wait());
}

TEST_F(AccountManagerFacadeImplTest,
       GetAccountsReturnsEmptyListOfAccountsWhenEmpty) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();

  base::test::TestFuture<const std::vector<Account>&> future;
  account_manager_facade->GetAccounts(future.GetCallback());
  EXPECT_THAT(future.Get(), testing::IsEmpty());
}

TEST_F(AccountManagerFacadeImplTest, GetAccountsReturnsAccounts) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  Account account1 = AddTestGaiaAccount(kTestAccountEmail, "token1");
  Account account2 = AddTestGaiaAccount(kAnotherTestAccountEmail, "token2");

  base::test::TestFuture<const std::vector<Account>&> future;
  account_manager_facade->GetAccounts(future.GetCallback());
  EXPECT_THAT(future.Get(), testing::UnorderedElementsAre(AccountEq(account1),
                                                          AccountEq(account2)));
}

TEST_F(AccountManagerFacadeImplTest, GetPersistentErrorMarshalsAuthErrorNone) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  Account account = AddTestGaiaAccount(kTestAccountEmail, "valid-token");

  base::test::TestFuture<const GoogleServiceAuthError&> future;
  account_manager_facade->GetPersistentErrorForAccount(account.key,
                                                       future.GetCallback());
  EXPECT_THAT(future.Get(), Eq(GoogleServiceAuthError::AuthErrorNone()));
}

TEST_F(AccountManagerFacadeImplTest,
       GetPersistentErrorMarshalsCredentialsRejectedByClient) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  Account account =
      AddTestGaiaAccount(kTestAccountEmail, AccountManager::kInvalidToken);

  base::test::TestFuture<const GoogleServiceAuthError&> future;
  account_manager_facade->GetPersistentErrorForAccount(account.key,
                                                       future.GetCallback());
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  EXPECT_THAT(future.Get(), Eq(expected_error));
}

TEST_F(AccountManagerFacadeImplTest,
       AccessTokenFetcherReturnsAnErrorForUninitializedRemote) {
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      mojo::Remote<crosapi::mojom::AccountManager>(),
      /*remote_version=*/std::numeric_limits<uint32_t>::max(),
      real_account_manager());
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  MockOAuthConsumer consumer;
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromServiceError("Mojo pipe disconnected");
  EXPECT_CALL(consumer, OnGetTokenFailure(Eq(error)));

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);

  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccountManagerFacadeImplTest,
       AccessTokenFetcherCanHandleMojoRemoteDisconnection) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  MockOAuthConsumer consumer;
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromServiceError("Mojo pipe disconnected");
  EXPECT_CALL(consumer, OnGetTokenFailure(Eq(error)));

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  account_manager().ClearReceivers();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccountManagerFacadeImplTest, AccessTokenFetchSucceeds) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(WithArgs<1>(&AccessTokenFetchSuccess));
  account_manager().SetMockAccessTokenFetcher(
      std::move(mock_access_token_fetcher));
  MockOAuthConsumer consumer;
  EXPECT_CALL(consumer,
              OnGetTokenSuccess(
                  Field(&OAuth2AccessTokenConsumer::TokenResponse::access_token,
                        Eq(kFakeAccessToken))));

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccountManagerFacadeImplTest, AccessTokenFetchErrorResponse) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(WithArgs<1>(&AccessTokenFetchServiceError));
  account_manager().SetMockAccessTokenFetcher(
      std::move(mock_access_token_fetcher));
  MockOAuthConsumer consumer;
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromServiceError(std::string());
  EXPECT_CALL(consumer, OnGetTokenFailure(Eq(error)));

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccountManagerFacadeImplTest,
       HistogramsForZeroAccountManagerRemoteDisconnections) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  // Expect 0 disconnections in the default state.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerRemote));

  // Reset the facade so that histograms get logged.
  account_manager_facade->FlushMojoForTesting();
  account_manager_facade.reset();

  // Expect 1 log - at the end of `account_manager_facade` destruction.
  histogram_tester().ExpectTotalCount(kMojoDisconnectionsAccountManagerRemote,
                                      1);
  // Expect 0 disconnections.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerRemote));
}

TEST_F(AccountManagerFacadeImplTest,
       HistogramsForAccountManagerRemoteDisconnection) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  // Expect 0 disconnections in the default state.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerRemote));

  // Simulate a disconnection.
  account_manager().ClearReceivers();
  // And reset the facade so that histograms get logged.
  account_manager_facade->FlushMojoForTesting();
  account_manager_facade.reset();

  // Expect 1 log - at the end of `account_manager_facade` destruction.
  histogram_tester().ExpectTotalCount(kMojoDisconnectionsAccountManagerRemote,
                                      1);
  // Expect 1 disconnection.
  EXPECT_EQ(1, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerRemote));
}

TEST_F(AccountManagerFacadeImplTest,
       HistogramsForZeroAccountManagerAccessTokenFetcherRemoteDisconnections) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(WithArgs<1>(&AccessTokenFetchSuccess));
  account_manager().SetMockAccessTokenFetcher(
      std::move(mock_access_token_fetcher));

  MockOAuthConsumer consumer;
  EXPECT_CALL(consumer,
              OnGetTokenSuccess(
                  Field(&OAuth2AccessTokenConsumer::TokenResponse::access_token,
                        Eq(kFakeAccessToken))));
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);
  // Expect 0 disconnections in the default state.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote));

  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  // Flush all pending Mojo messages.
  base::RunLoop().RunUntilIdle();
  // Reset the fetcher so that histograms get logged.
  access_token_fetcher.reset();

  // Expect 1 log - at the end of `account_manager_facade` destruction.
  histogram_tester().ExpectTotalCount(
      kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote, 1);
  // Expect 0 disconnections.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote));
}

TEST_F(AccountManagerFacadeImplTest,
       HistogramsForAccountManagerAccessTokenFetcherRemoteDisconnections) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  // Create a mock access token fetcher that closes its receiver end of the Mojo
  // pipe as soon as its `Start()` method is called with any parameters.
  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(Invoke(mock_access_token_fetcher.get(),
                       &MockAccessTokenFetcher::ResetReceiver));
  account_manager().SetMockAccessTokenFetcher(
      std::move(mock_access_token_fetcher));

  MockOAuthConsumer consumer;
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);
  // Expect 0 disconnections in the default state.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote));

  // Calling `Start` will reset the Mojo connection from the receiver side. This
  // should notify the remote side, and result in a histogram log.
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  // Flush all pending Mojo messages.
  base::RunLoop().RunUntilIdle();
  // Reset the fetcher so that histograms get logged.
  access_token_fetcher.reset();

  // Expect 1 log - at the end of `account_manager_facade` destruction.
  histogram_tester().ExpectTotalCount(
      kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote, 1);
  // Expect 1 disconnection.
  EXPECT_EQ(1, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote));
}

TEST_F(AccountManagerFacadeImplTest, ReportAuthError) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  testing::StrictMock<MockAccountManagerFacadeObserver> observer;
  base::ScopedObservation<AccountManagerFacade, AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(account_manager_facade.get());

  Account account = CreateTestGaiaAccount(kTestAccountEmail);
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer, OnAuthErrorChanged(account.key, error))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  account_manager_facade->ReportAuthError(account.key, error);
  EXPECT_TRUE(future.Wait());
}

TEST_F(AccountManagerFacadeImplTest, ReportAuthErrorIgnoresTransientErrors) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  testing::StrictMock<MockAccountManagerFacadeObserver> observer;
  base::ScopedObservation<AccountManagerFacade, AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(account_manager_facade.get());

  Account account = CreateTestGaiaAccount(kTestAccountEmail);
  for (auto state : {GoogleServiceAuthError::CONNECTION_FAILED,
                     GoogleServiceAuthError::SERVICE_UNAVAILABLE,
                     GoogleServiceAuthError::REQUEST_CANCELED,
                     GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED}) {
    GoogleServiceAuthError error(state);
    ASSERT_TRUE(error.IsTransientError());
    // `observer` is a StrictMock; test will fail if any method is called.
    account_manager_facade->ReportAuthError(account.key, error);
  }
}

TEST_F(AccountManagerFacadeImplTest, GetAccountsIsAlwaysAsynchronous) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();

  bool callback_called = false;
  account_manager_facade->GetAccounts(base::BindLambdaForTesting(
      [&callback_called](const std::vector<Account>& accounts) {
        callback_called = true;
      }));

  // The callback must not be called synchronously.
  EXPECT_FALSE(callback_called);

  // Wait for the task to run.
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_called; }));
}

}  // namespace account_manager
