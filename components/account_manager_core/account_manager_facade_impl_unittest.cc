// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade_impl.h"

#include <limits>

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_manager_test_util.h"
#include "components/account_manager_core/account_manager_util.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFakeEmail[] = "fake_email@example.com";

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
    std::transform(std::begin(accounts_), std::end(accounts_),
                   std::back_inserter(mojo_accounts),
                   &account_manager::ToMojoAccount);
    std::move(callback).Run(std::move(mojo_accounts));
  }

  void ShowAddAccountDialog(ShowAddAccountDialogCallback callback) override {
    show_add_account_dialog_calls_++;
    std::move(callback).Run(account_manager::ToMojoAccountAdditionResult(
        account_manager::AccountAdditionResult(
            account_manager::AccountAdditionResult::Status::kCancelledByUser)));
  }

  void ShowReauthAccountDialog(const std::string& email,
                               base::OnceClosure closure) override {
    show_reauth_account_dialog_calls_++;
    std::move(closure).Run();
  }

  mojo::Remote<crosapi::mojom::AccountManager> CreateRemote() {
    mojo::Remote<crosapi::mojom::AccountManager> remote;
    receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  void NotifyOnTokenUpsertedObservers(const account_manager::Account& account) {
    for (auto& observer : observers_) {
      observer->OnTokenUpserted(ToMojoAccount(account));
    }
  }

  void NotifyOnAccountRemovedObservers(
      const account_manager::Account& account) {
    for (auto& observer : observers_) {
      observer->OnAccountRemoved(ToMojoAccount(account));
    }
  }

  void SetAccounts(const std::vector<account_manager::Account>& accounts) {
    accounts_ = accounts;
  }

  int show_add_account_dialog_calls() { return show_add_account_dialog_calls_; }

  int show_reauth_account_dialog_calls() {
    return show_reauth_account_dialog_calls_;
  }

 private:
  int show_add_account_dialog_calls_ = 0;
  int show_reauth_account_dialog_calls_ = 0;
  bool is_initialized_{false};
  std::vector<account_manager::Account> accounts_;
  mojo::ReceiverSet<crosapi::mojom::AccountManager> receivers_;
  mojo::RemoteSet<crosapi::mojom::AccountManagerObserver> observers_;
};

class MockObserver : public account_manager::AccountManagerFacade::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnAccountUpserted,
              (const account_manager::AccountKey& account),
              (override));
  MOCK_METHOD(void,
              OnAccountRemoved,
              (const account_manager::AccountKey& account),
              (override));
};

MATCHER_P(AccountEq, expected_account, "") {
  return testing::ExplainMatchResult(
             testing::Field(&account_manager::Account::key,
                            testing::Eq(expected_account.key)),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Field(&account_manager::Account::raw_email,
                            testing::StrEq(expected_account.raw_email)),
             arg, result_listener);
}

using base::MockOnceCallback;

constexpr char kTestAccountEmail[] = "test@gmail.com";
constexpr char kAnotherTestAccountEmail[] = "another_test@gmail.com";

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

  std::unique_ptr<AccountManagerFacadeImpl> CreateFacade() {
    base::RunLoop run_loop;
    auto result = std::make_unique<AccountManagerFacadeImpl>(
        account_manager().CreateRemote(),
        /* remote_version= */ std::numeric_limits<uint32_t>::max(),
        run_loop.QuitClosure());
    run_loop.Run();
    return result;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeAccountManager account_manager_;
};

TEST_F(AccountManagerFacadeImplTest,
       FacadeIsInitializedOnConnectIfAccountManagerIsInitialized) {
  account_manager().SetIsInitialized(true);

  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  EXPECT_TRUE(account_manager_facade->IsInitialized());
}

TEST_F(AccountManagerFacadeImplTest, FacadeIsUninitializedByDefault) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  EXPECT_FALSE(account_manager_facade->IsInitialized());
}

TEST_F(AccountManagerFacadeImplTest,
       UninitializedFacadeIsInitializedOnFirstTokenUpsertedNotification) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  ASSERT_FALSE(account_manager_facade->IsInitialized());

  testing::StrictMock<MockObserver> observer;
  account_manager_facade->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAccountUpserted(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  account_manager().NotifyOnTokenUpsertedObservers(
      account_manager::CreateTestGaiaAccount(kTestAccountEmail));
  run_loop.Run();

  EXPECT_TRUE(account_manager_facade->IsInitialized());
}

TEST_F(AccountManagerFacadeImplTest, OnTokenUpsertedIsPropagatedToObservers) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  testing::StrictMock<MockObserver> observer;
  account_manager_facade->AddObserver(&observer);

  account_manager::Account account =
      account_manager::CreateTestGaiaAccount(kTestAccountEmail);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAccountUpserted(account.key))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  account_manager().NotifyOnTokenUpsertedObservers(account);
  run_loop.Run();
}

TEST_F(AccountManagerFacadeImplTest, OnAccountRemovedIsPropagatedToObservers) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  testing::StrictMock<MockObserver> observer;
  account_manager_facade->AddObserver(&observer);

  account_manager::Account account =
      account_manager::CreateTestGaiaAccount(kTestAccountEmail);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAccountRemoved(account.key))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  account_manager().NotifyOnAccountRemovedObservers(account);
  run_loop.Run();
}

TEST_F(AccountManagerFacadeImplTest,
       GetAccountsReturnsEmptyListOfAccountsWhenAccountManagerAshIsEmpty) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  account_manager().SetAccounts({});

  MockOnceCallback<void(const std::vector<account_manager::Account>&)> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::IsEmpty()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  account_manager_facade->GetAccounts(callback.Get());
  run_loop.Run();
}

TEST_F(AccountManagerFacadeImplTest, GetAccountsCorrectlyMarshalsTwoAccounts) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  account_manager::Account account1 =
      account_manager::CreateTestGaiaAccount(kTestAccountEmail);
  account_manager::Account account2 =
      account_manager::CreateTestGaiaAccount(kAnotherTestAccountEmail);
  account_manager().SetAccounts({account1, account2});

  MockOnceCallback<void(const std::vector<account_manager::Account>&)> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::ElementsAre(AccountEq(account1),
                                                 AccountEq(account2))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  account_manager_facade->GetAccounts(callback.Get());
  run_loop.Run();
}

TEST_F(AccountManagerFacadeImplTest,
       GetAccountsIsSafeToCallBeforeAccountManagerFacadeIsNotInitialized) {
  account_manager::Account account =
      account_manager::CreateTestGaiaAccount(kTestAccountEmail);
  account_manager().SetAccounts({account});

  // |CreateFacade| waits for the AccountManagerFacadeImpl's initialization
  // sequence to be finished. To avoid this, create it directly here.
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      account_manager().CreateRemote(),
      /* remote_version= */ std::numeric_limits<uint32_t>::max());

  MockOnceCallback<void(const std::vector<account_manager::Account>&)> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::ElementsAre(AccountEq(account))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  account_manager_facade->GetAccounts(callback.Get());
  run_loop.Run();
}

TEST_F(AccountManagerFacadeImplTest,
       GetAccountsReturnsEmptyListOfAccountsWhenRemoteIsNull) {
  auto account_manager_facade = std::make_unique<AccountManagerFacadeImpl>(
      mojo::Remote<crosapi::mojom::AccountManager>(),
      /* remote_version= */ std::numeric_limits<uint32_t>::max());

  MockOnceCallback<void(const std::vector<account_manager::Account>&)> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::IsEmpty()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  account_manager_facade->GetAccounts(callback.Get());
  run_loop.Run();
}

TEST_F(AccountManagerFacadeImplTest, ShowAddAccountDialogCallsMojo) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  EXPECT_EQ(0, account_manager().show_add_account_dialog_calls());
  account_manager_facade->ShowAddAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kSettingsAddAccountButton,
      base::BindOnce(
          [](const account_manager::AccountAdditionResult& result) {}));
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_add_account_dialog_calls());
}

TEST_F(AccountManagerFacadeImplTest, ShowReauthAccountDialogCallsMojo) {
  std::unique_ptr<AccountManagerFacadeImpl> account_manager_facade =
      CreateFacade();
  EXPECT_EQ(0, account_manager().show_reauth_account_dialog_calls());
  account_manager_facade->ShowReauthAccountDialog(
      account_manager::AccountManagerFacade::AccountAdditionSource::
          kSettingsAddAccountButton,
      kFakeEmail);
  account_manager_facade->FlushMojoForTesting();
  EXPECT_EQ(1, account_manager().show_reauth_account_dialog_calls());
}
