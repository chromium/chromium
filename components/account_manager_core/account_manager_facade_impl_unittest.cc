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
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_manager_test_util.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
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

  AccountManager* real_account_manager() { return real_account_manager_.get(); }

  std::unique_ptr<AccountManagerFacadeImpl> CreateFacade() {
    return std::make_unique<AccountManagerFacadeImpl>(real_account_manager());
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
