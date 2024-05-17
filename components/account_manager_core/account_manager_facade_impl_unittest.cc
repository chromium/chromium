// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade_impl.h"

#include <limits>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_manager_test_util.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
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
constexpr char kMojoDisconnectionsAccountManagerObserverReceiver[] =
    "AccountManager.MojoDisconnections.AccountManagerObserverReceiver";
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
              GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR)));
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

  void IsInitialized(IsInitializedCallback cb) override {
    std::move(cb).Run(is_initialized_);
  }

  void SetIsInitialized(bool is_initialized) {
    is_initialized_ = is_initialized;
  }

  void AddObserver(AddObserverCallback cb) override {
    mojo::Remote<crosapi::mojom::AccountManagerObserver> observer;
    std::move(cb).Run(observer.BindNewPipeAndPassReceiver());
    observers_.Add(std::move(observer));
  }

  void GetAccounts(GetAccountsCallback callback) override {
    std::vector<crosapi::mojom::AccountPtr> mojo_accounts;
    base::ranges::transform(accounts_, std::back_inserter(mojo_accounts),
                            &ToMojoAccount);
    std::move(callback).Run(std::move(mojo_accounts));
  }

  void GetPersistentErrorForAccount(
      crosapi::mojom::AccountKeyPtr mojo_account_key,
      GetPersistentErrorForAccountCallback callback) override {
    std::optional<AccountKey> account_key =
        FromMojoAccountKey(mojo_account_key);
    DCHECK(account_key.has_value());
    auto it = persistent_errors_.find(account_key.value());
    if (it != persistent_errors_.end()) {
      std::move(callback).Run(ToMojoGoogleServiceAuthError(it->second));
      return;
    }
    std::move(callback).Run(
        ToMojoGoogleServiceAuthError(GoogleServiceAuthError::AuthErrorNone()));
  }

  void ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptionsPtr options,
                            ShowAddAccountDialogCallback callback) override {
    show_add_account_dialog_calls_++;
    show_add_account_dialog_options_ = FromMojoAccountAdditionOptions(options);
    std::move(callback).Run(
        account_manager::ToMojoAccountUpsertionResult(*upsertion_result_));
  }

  void ShowReauthAccountDialog(
      const std::string& email,
      ShowReauthAccountDialogCallback callback) override {
    show_reauth_account_dialog_calls_++;
    std::move(callback).Run(
        account_manager::ToMojoAccountUpsertionResult(*upsertion_result_));
  }

  void ShowManageAccountsSettings() override {
    show_manage_accounts_settings_calls_++;
  }

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

  void ReportAuthError(
      crosapi::mojom::AccountKeyPtr account,
      crosapi::mojom::GoogleServiceAuthErrorPtr error) override {
    for (auto& observer : observers_) {
      observer->OnAuthErrorChanged(account->Clone(), error->Clone());
    }
  }

  mojo::Remote<crosapi::mojom::AccountManager> CreateRemote() {
    mojo::Remote<crosapi::mojom::AccountManager> remote;
    receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  void NotifyOnTokenUpsertedObservers(const Account& account) {
    for (auto& observer : observers_) {
      observer->OnTokenUpserted(ToMojoAccount(account));
    }
  }

  void NotifyOnAccountRemovedObservers(const Account& account) {
    for (auto& observer : observers_) {
      observer->OnAccountRemoved(ToMojoAccount(account));
    }
  }

  void SetAccounts(const std::vector<Account>& accounts) {
    accounts_ = accounts;
  }

  void SetPersistentErrorForAccount(const AccountKey& account,
                                    GoogleServiceAuthError error) {
    persistent_errors_.emplace(account, error);
  }

  void SetAccountUpsertionResult(
      const account_manager::AccountUpsertionResult& result) {
    upsertion_result_ = std::make_unique<AccountUpsertionResult>(result);
  }

  void ClearReceivers() { receivers_.Clear(); }

  void ClearObservers() { observers_.Clear(); }

  int show_add_account_dialog_calls() const {
    return show_add_account_dialog_calls_;
  }

  std::optional<account_manager::AccountAdditionOptions>
  show_add_account_dialog_options() const {
    return show_add_account_dialog_options_;
  }

  int show_reauth_account_dialog_calls() const {
    return show_reauth_account_dialog_calls_;
  }

  int show_manage_accounts_settings_calls() const {
    return show_manage_accounts_settings_calls_;
  }

 private:
  int show_add_account_dialog_calls_ = 0;
  std::optional<account_manager::AccountAdditionOptions>
      show_add_account_dialog_options_;
  int show_reauth_account_dialog_calls_ = 0;
  int show_manage_accounts_settings_calls_ = 0;
  bool is_initialized_ = false;
  std::vector<Account> accounts_;
  std::map<AccountKey, GoogleServiceAuthError> persistent_errors_;
  std::unique_ptr<AccountUpsertionResult> upsertion_result_;
  std::unique_ptr<MockAccessTokenFetcher> access_token_fetcher_;
  mojo::ReceiverSet<crosapi::mojom::AccountManager> receivers_;
  mojo::RemoteSet<crosapi::mojom::AccountManagerObserver> observers_;
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
  FakeAccountManager& account_manager() { return account_manager_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  std::unique_ptr<AccountManagerFacadeImpl> CreateFacade() {
    base::test::TestFuture<void> future;
    auto result = std::make_unique<AccountManagerFacadeImpl>(
        account_manager().CreateRemote(),
        /*remote_version=*/std::numeric_limits<uint32_t>::max(),
        /*account_manager_for_tests=*/nullptr, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return result;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeAccountManager account_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AccountManagerFacadeImplTest, InitializationStatusIsCorrectlySet) {
  // This will wait for an initialization callback to be called.
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  EXPECT_TRUE(account_manager_facade->IsInitialized());
}

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
  account_manager().NotifyOnTokenUpsertedObservers(account);
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
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer, OnAccountRemoved(AccountEq(account)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  account_manager().NotifyOnAccountRemovedObservers(account);
  EXPECT_TRUE(future.Wait());
}

TEST_F(
    AccountManagerFacadeImplTest,
    GetAccountsReturnsEmptyListOfAccountsWhenAccountManagerMojoServiceIsEmpty) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  account_manager().SetAccounts({});

  base::test::TestFuture<const std::vector<Account>&> future;
  account_manager_facade->GetAccounts(future.GetCallback());
  EXPECT_THAT(future.Get(), testing::IsEmpty());
}

TEST_F(AccountManagerFacadeImplTest, GetAccountsCorrectlyMarshalsTwoAccounts) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  Account account1 = CreateTestGaiaAccount(kTestAccountEmail);
  Account account2 = CreateTestGaiaAccount(kAnotherTestAccountEmail);
  account_manager().SetAccounts({account1, account2});

  base::test::TestFuture<const std::vector<Account>&> future;
  account_manager_facade->GetAccounts(future.GetCallback());
  EXPECT_THAT(future.Get(),
              testing::ElementsAre(AccountEq(account1), AccountEq(account2)));
}

TEST_F(AccountManagerFacadeImplTest,
       GetAccountsIsSafeToCallBeforeAccountManagerFacadeIsInitialized) {
  Account account = CreateTestGaiaAccount(kTestAccountEmail);
  account_manager().SetAccounts({account});

  // |CreateFacade| waits for the AccountManagerFacadeImpl's initialization
  // sequence to be finished. To avoid this, create it directly here.
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      account_manager().CreateRemote(),
      /*remote_version=*/std::numeric_limits<uint32_t>::max(),
      /*account_manager_for_tests=*/nullptr);

  base::test::TestFuture<const std::vector<Account>&> future;
  account_manager_facade->GetAccounts(future.GetCallback());
  EXPECT_THAT(future.Get(), testing::ElementsAre(AccountEq(account)));
}

// Regression test for https://crbug.com/1287297
// Do not return empty accounts when the remote is not available.
TEST_F(AccountManagerFacadeImplTest, GetAccountsHangsWhenRemoteIsNull) {
  base::HistogramTester tester;
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      mojo::Remote<crosapi::mojom::AccountManager>(),
      /*remote_version=*/std::numeric_limits<uint32_t>::max(),
      /*account_manager_for_tests=*/nullptr);

  bool callback_was_dropped = false;
  // scoped_closure that sets `callback_was_dropped` when it is destroyed.
  base::ScopedClosureRunner scoped_closure(base::BindLambdaForTesting(
      [&callback_was_dropped]() { callback_was_dropped = true; }));
  // Pass ownership of the scoped closure to the main callback, so that the
  // scoped closure is run when the callback is destroyed.
  // This callback should not be run.
  base::OnceCallback<void(const std::vector<Account>&)> dropped_callback =
      base::BindLambdaForTesting(
          [scoped_closure = std::move(scoped_closure)](
              const std::vector<Account>&) { NOTREACHED_IN_MIGRATION(); });
  EXPECT_FALSE(callback_was_dropped);
  account_manager_facade->GetAccounts(std::move(dropped_callback));
  // `dropped_callback` was destroyed without being run.
  EXPECT_TRUE(callback_was_dropped);

  tester.ExpectUniqueSample(
      AccountManagerFacadeImpl::GetAccountsMojoStatusHistogramNameForTesting(),
      /*sample=*/AccountManagerFacadeImpl::FacadeMojoStatus::kNoRemote,
      /*expected_count=*/1);
}

TEST_F(AccountManagerFacadeImplTest, GetPersistentErrorMarshalsAuthErrorNone) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  Account account = CreateTestGaiaAccount(kTestAccountEmail);

  base::test::TestFuture<const GoogleServiceAuthError&> future;
  account_manager_facade->GetPersistentErrorForAccount(account.key,
                                                       future.GetCallback());
  EXPECT_THAT(future.Get(), Eq(GoogleServiceAuthError::AuthErrorNone()));
}

TEST_F(AccountManagerFacadeImplTest,
       GetPersistentErrorMarshalsCredentialsRejectedByClient) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  Account account = CreateTestGaiaAccount(kTestAccountEmail);
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  account_manager().SetPersistentErrorForAccount(account.key, error);

  base::test::TestFuture<const GoogleServiceAuthError&> future;
  account_manager_facade->GetPersistentErrorForAccount(account.key,
                                                       future.GetCallback());
  EXPECT_THAT(future.Get(), Eq(error));
}

TEST_F(AccountManagerFacadeImplTest, ShowAddAccountDialogCallsMojo) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  account_manager().SetAccountUpsertionResult(
      account_manager::AccountUpsertionResult::FromStatus(
          account_manager::AccountUpsertionResult::Status::
              kUnexpectedResponse));
  EXPECT_EQ(0, account_manager().show_add_account_dialog_calls());
  account_manager_facade->ShowAddAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kSettingsAddAccountButton);
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_add_account_dialog_calls());
}

TEST_F(AccountManagerFacadeImplTest,
       ShowAddAccountDialogSetsCorrectOptionsForAdditionFromAsh) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  account_manager().SetAccountUpsertionResult(
      account_manager::AccountUpsertionResult::FromStatus(
          account_manager::AccountUpsertionResult::Status::
              kUnexpectedResponse));
  EXPECT_EQ(0, account_manager().show_add_account_dialog_calls());
  account_manager_facade->ShowAddAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kSettingsAddAccountButton);
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_add_account_dialog_calls());
  EXPECT_TRUE(account_manager().show_add_account_dialog_options().has_value());
  EXPECT_TRUE(
      account_manager().show_add_account_dialog_options()->is_available_in_arc);
  EXPECT_FALSE(account_manager()
                   .show_add_account_dialog_options()
                   ->show_arc_availability_picker);
}

TEST_F(AccountManagerFacadeImplTest,
       ShowAddAccountDialogSetsCorrectOptionsForAdditionFromLacros) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  account_manager().SetAccountUpsertionResult(
      account_manager::AccountUpsertionResult::FromStatus(
          account_manager::AccountUpsertionResult::Status::
              kUnexpectedResponse));
  EXPECT_EQ(0, account_manager().show_add_account_dialog_calls());
  account_manager_facade->ShowAddAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kOgbAddAccount);
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_add_account_dialog_calls());
  EXPECT_TRUE(account_manager().show_add_account_dialog_options().has_value());
  EXPECT_FALSE(
      account_manager().show_add_account_dialog_options()->is_available_in_arc);
  EXPECT_FALSE(account_manager()
                   .show_add_account_dialog_options()
                   ->show_arc_availability_picker);
}

TEST_F(AccountManagerFacadeImplTest,
       ShowAddAccountDialogSetsCorrectOptionsForAdditionFromArc) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  account_manager().SetAccountUpsertionResult(
      account_manager::AccountUpsertionResult::FromStatus(
          account_manager::AccountUpsertionResult::Status::
              kUnexpectedResponse));
  EXPECT_EQ(0, account_manager().show_add_account_dialog_calls());
  account_manager_facade->ShowAddAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::kArc);
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_add_account_dialog_calls());
  EXPECT_TRUE(account_manager().show_add_account_dialog_options().has_value());
  EXPECT_TRUE(
      account_manager().show_add_account_dialog_options()->is_available_in_arc);
  EXPECT_TRUE(account_manager()
                  .show_add_account_dialog_options()
                  ->show_arc_availability_picker);
}

TEST_F(AccountManagerFacadeImplTest, ShowAddAccountDialogUMA) {
  base::HistogramTester tester;
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  auto result = account_manager::AccountUpsertionResult::FromStatus(
      account_manager::AccountUpsertionResult::Status::kAlreadyInProgress);
  account_manager().SetAccountUpsertionResult(result);
  auto source = account_manager::AccountManagerFacade::AccountAdditionSource::
      kSettingsAddAccountButton;

  account_manager_facade->ShowAddAccountDialog(source);
  account_manager_facade->FlushMojoForTesting();

  // Check that UMA stats were sent.
  tester.ExpectUniqueSample(
      account_manager::AccountManagerFacade::kAccountAdditionSource,
      /*sample=*/source, /*expected_count=*/1);
  tester.ExpectUniqueSample(
      AccountManagerFacadeImpl::
          GetAccountUpsertionResultStatusHistogramNameForTesting(),
      /*sample=*/result.status(), /*expected_count=*/1);
}

TEST_F(AccountManagerFacadeImplTest,
       ShowAddAccountDialogReturnsAnErrorIfMojoRemoteIsDisconnected) {
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      mojo::Remote<crosapi::mojom::AccountManager>(),
      /*remote_version=*/std::numeric_limits<uint32_t>::max(),
      /*account_manager_for_tests=*/nullptr);

  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;
  account_manager_facade->ShowAddAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kSettingsAddAccountButton,
      future.GetCallback());
  account_manager::AccountUpsertionResult result = future.Get();
  EXPECT_EQ(
      account_manager::AccountUpsertionResult::Status::kMojoRemoteDisconnected,
      result.status());
}

TEST_F(AccountManagerFacadeImplTest,
       ShowAddAccountDialogReturnsAnErrorIfMojoVersionIsIncompatible) {
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      account_manager().CreateRemote(),
      /*remote_version=*/1,
      /*account_manager_for_tests=*/nullptr);

  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;
  account_manager_facade->ShowAddAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kSettingsAddAccountButton,
      future.GetCallback());
  account_manager::AccountUpsertionResult result = future.Get();
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::
                kIncompatibleMojoVersions,
            result.status());
}

TEST_F(AccountManagerFacadeImplTest, ShowReauthAccountDialogCallsMojo) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();

  account_manager().SetAccountUpsertionResult(
      account_manager::AccountUpsertionResult::FromStatus(
          account_manager::AccountUpsertionResult::Status::
              kUnexpectedResponse));
  EXPECT_EQ(0, account_manager().show_reauth_account_dialog_calls());
  account_manager_facade->ShowReauthAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kContentAreaReauth,
      kTestAccountEmail, base::DoNothing());
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_reauth_account_dialog_calls());
}

TEST_F(AccountManagerFacadeImplTest, ShowReauthAccountDialogUMA) {
  base::HistogramTester tester;
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();

  auto result = account_manager::AccountUpsertionResult::FromStatus(
      account_manager::AccountUpsertionResult::Status::kAlreadyInProgress);
  account_manager().SetAccountUpsertionResult(result);
  auto source = AccountManagerFacade::AccountAdditionSource::kContentAreaReauth;

  account_manager_facade->ShowReauthAccountDialog(source, kTestAccountEmail,
                                                  base::DoNothing());
  account_manager_facade->FlushMojoForTesting();

  // Check that UMA stats were sent.
  tester.ExpectUniqueSample(AccountManagerFacade::kAccountAdditionSource,
                            /*sample=*/source, /*expected_count=*/1);
}

TEST_F(AccountManagerFacadeImplTest, ShowManageAccountsSettingsCallsMojo) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  EXPECT_EQ(0, account_manager().show_manage_accounts_settings_calls());
  account_manager_facade->ShowManageAccountsSettings();
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_manage_accounts_settings_calls());
}

TEST_F(AccountManagerFacadeImplTest,
       AccessTokenFetcherReturnsAnErrorForUninitializedRemote) {
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      mojo::Remote<crosapi::mojom::AccountManager>(),
      /*remote_version=*/std::numeric_limits<uint32_t>::max(),
      /*account_manager_for_tests=*/nullptr);
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
       AccessTokenFetcherCanBeCreatedBeforeAccountManagerFacadeInitialization) {
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      account_manager().CreateRemote(),
      /*remote_version=*/std::numeric_limits<uint32_t>::max(),
      /*account_manager_for_tests=*/nullptr);
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(WithArgs<1>(Invoke(&AccessTokenFetchSuccess)));
  account_manager().SetMockAccessTokenFetcher(
      std::move(mock_access_token_fetcher));
  MockOAuthConsumer consumer;

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);
  EXPECT_FALSE(account_manager_facade->IsInitialized());
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  EXPECT_CALL(consumer,
              OnGetTokenSuccess(
                  Field(&OAuth2AccessTokenConsumer::TokenResponse::access_token,
                        Eq(kFakeAccessToken))));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_manager_facade->IsInitialized());
}

TEST_F(AccountManagerFacadeImplTest,
       AccessTokenFetcherCanHandleMojoRemoteDisconnection) {
  account_manager().SetIsInitialized(true);
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
  account_manager().SetIsInitialized(true);
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(WithArgs<1>(Invoke(&AccessTokenFetchSuccess)));
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
  account_manager().SetIsInitialized(true);
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(WithArgs<1>(Invoke(&AccessTokenFetchServiceError)));
  account_manager().SetMockAccessTokenFetcher(
      std::move(mock_access_token_fetcher));
  MockOAuthConsumer consumer;
  GoogleServiceAuthError error(GoogleServiceAuthError::SERVICE_ERROR);
  EXPECT_CALL(consumer, OnGetTokenFailure(Eq(error)));

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager_facade->CreateAccessTokenFetcher(account.key, &consumer);
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccountManagerFacadeImplTest,
       HistogramsForZeroAccountManagerRemoteDisconnections) {
  account_manager().SetIsInitialized(true);
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
  account_manager().SetIsInitialized(true);
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
       HistogramsForZeroAccountManagerObserverReceiverDisconnections) {
  account_manager().SetIsInitialized(true);
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  // Expect 0 disconnections in the default state.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerObserverReceiver));

  // Reset the facade so that histograms get logged.
  account_manager_facade->FlushMojoForTesting();
  account_manager_facade.reset();

  // Expect 1 log - at the end of `account_manager_facade` destruction.
  histogram_tester().ExpectTotalCount(
      kMojoDisconnectionsAccountManagerObserverReceiver, 1);
  // Expect 0 disconnections.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerObserverReceiver));
}

TEST_F(AccountManagerFacadeImplTest,
       HistogramsForAccountManagerObserverReceiverDisconnections) {
  account_manager().SetIsInitialized(true);
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  // Expect 0 disconnections in the default state.
  EXPECT_EQ(0, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerObserverReceiver));

  // Simulate a disconnection.
  account_manager().ClearObservers();
  // And reset the facade so that histograms get logged.
  account_manager_facade->FlushMojoForTesting();
  account_manager_facade.reset();

  // Expect 1 log - at the end of `account_manager_facade` destruction.
  histogram_tester().ExpectTotalCount(
      kMojoDisconnectionsAccountManagerObserverReceiver, 1);
  // Expect 1 disconnection.
  EXPECT_EQ(1, histogram_tester().GetTotalSum(
                   kMojoDisconnectionsAccountManagerObserverReceiver));
}

TEST_F(AccountManagerFacadeImplTest,
       HistogramsForZeroAccountManagerAccessTokenFetcherRemoteDisconnections) {
  account_manager().SetIsInitialized(true);
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  const Account account = CreateTestGaiaAccount(kTestAccountEmail);

  auto mock_access_token_fetcher = std::make_unique<MockAccessTokenFetcher>();
  EXPECT_CALL(*mock_access_token_fetcher.get(), Start(_, _))
      .WillOnce(WithArgs<1>(Invoke(&AccessTokenFetchSuccess)));
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
  account_manager().SetIsInitialized(true);
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

TEST_F(AccountManagerFacadeImplTest,
       SigninDialogClosureNotificationsAreReported) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  testing::StrictMock<MockAccountManagerFacadeObserver> observer;
  base::ScopedObservation<AccountManagerFacade, AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(account_manager_facade.get());

  base::test::TestFuture<void> future;
  EXPECT_CALL(observer, OnSigninDialogClosed)
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  account_manager_facade->OnSigninDialogClosed();
  EXPECT_TRUE(future.Wait());
}

}  // namespace account_manager
