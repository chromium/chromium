// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/account_manager/account_manager.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace chromeos {

namespace {

constexpr char kGaiaToken[] = "gaia_token";
constexpr char kNewGaiaToken[] = "new_gaia_token";
constexpr char kRawUserEmail[] = "user@example.com";

bool IsAccountKeyPresent(const std::vector<AccountManager::Account>& accounts,
                         const AccountManager::AccountKey& account_key) {
  for (const auto& account : accounts) {
    if (account.key == account_key) {
      return true;
    }
  }

  return false;
}

}  // namespace

class AccountManagerSpy : public AccountManager {
 public:
  AccountManagerSpy() = default;
  ~AccountManagerSpy() override = default;

  MOCK_METHOD1(RevokeGaiaTokenOnServer, void(const std::string& refresh_token));

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerSpy);
};

class AccountManagerTest : public testing::Test {
 public:
  AccountManagerTest() {}
  ~AccountManagerTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    AccountManager::RegisterPrefs(pref_service_.registry());
    ResetAndInitializeAccountManager();
  }

  // Gets the list of accounts stored in |account_manager_|.
  std::vector<AccountManager::Account> GetAccountsBlocking() {
    std::vector<AccountManager::Account> accounts;

    base::RunLoop run_loop;
    account_manager_->GetAccounts(base::BindLambdaForTesting(
        [&accounts, &run_loop](
            const std::vector<AccountManager::Account>& stored_accounts) {
          accounts = stored_accounts;
          run_loop.Quit();
        }));
    run_loop.Run();

    return accounts;
  }

  // Gets the raw email for |account_key|.
  std::string GetAccountEmailBlocking(
      const AccountManager::AccountKey& account_key) {
    std::string raw_email;

    base::RunLoop run_loop;
    account_manager_->GetAccountEmail(
        account_key,
        base::BindLambdaForTesting(
            [&raw_email, &run_loop](const std::string& stored_raw_email) {
              raw_email = stored_raw_email;
              run_loop.Quit();
            }));
    run_loop.Run();

    return raw_email;
  }

  // Helper method to reset and initialize |account_manager_| with default
  // parameters.
  void ResetAndInitializeAccountManager() {
    account_manager_ = std::make_unique<AccountManagerSpy>();
    InitializeAccountManager(account_manager_.get(), base::DoNothing());
  }

  // |account_manager| is a non-owning pointer.
  void InitializeAccountManager(AccountManager* account_manager,
                                base::OnceClosure initialization_callback) {
    account_manager->Initialize(
        tmp_dir_.GetPath(), test_url_loader_factory_.GetSafeWeakWrapper(),
        immediate_callback_runner_, base::SequencedTaskRunnerHandle::Get(),
        std::move(initialization_callback));
    account_manager->SetPrefService(&pref_service_);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(account_manager->init_state_,
              AccountManager::InitializationState::kInitialized);
    EXPECT_TRUE(account_manager->IsInitialized());
  }

  // Check base/test/task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir tmp_dir_;
  TestingPrefServiceSimple pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<AccountManagerSpy> account_manager_;
  const AccountManager::AccountKey kGaiaAccountKey_{
      "gaia_id", account_manager::AccountType::ACCOUNT_TYPE_GAIA};
  const AccountManager::AccountKey kActiveDirectoryAccountKey_{
      "object_guid",
      account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY};

  AccountManager::DelayNetworkCallRunner immediate_callback_runner_ =
      base::BindRepeating(
          [](base::OnceClosure closure) -> void { std::move(closure).Run(); });

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerTest);
};

class AccountManagerObserver : public AccountManager::Observer {
 public:
  AccountManagerObserver() = default;
  ~AccountManagerObserver() override = default;

  void OnTokenUpserted(const AccountManager::Account& account) override {
    is_token_upserted_callback_called_ = true;
    accounts_.insert(account.key);
    last_upserted_account_key_ = account.key;
    last_upserted_account_email_ = account.raw_email;
  }

  void OnAccountRemoved(const AccountManager::Account& account) override {
    is_account_removed_callback_called_ = true;
    accounts_.erase(account.key);
    last_removed_account_key_ = account.key;
    last_removed_account_email_ = account.raw_email;
  }

  bool is_token_upserted_callback_called_ = false;
  bool is_account_removed_callback_called_ = false;
  AccountManager::AccountKey last_upserted_account_key_;
  std::string last_upserted_account_email_;
  AccountManager::AccountKey last_removed_account_key_;
  std::string last_removed_account_email_;
  std::set<AccountManager::AccountKey> accounts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerObserver);
};

TEST(AccountManagerKeyTest, TestValidity) {
  AccountManager::AccountKey key1{
      std::string(), account_manager::AccountType::ACCOUNT_TYPE_GAIA};
  EXPECT_FALSE(key1.IsValid());

  AccountManager::AccountKey key2{
      "abc", account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED};
  EXPECT_FALSE(key2.IsValid());

  AccountManager::AccountKey key3{
      "abc", account_manager::AccountType::ACCOUNT_TYPE_GAIA};
  EXPECT_TRUE(key3.IsValid());
}

TEST_F(AccountManagerTest, TestInitializationCompletes) {
  AccountManager account_manager;

  EXPECT_EQ(account_manager.init_state_,
            AccountManager::InitializationState::kNotStarted);
  // Test assertions will be made inside the method.
  InitializeAccountManager(&account_manager, base::DoNothing());
}

TEST_F(AccountManagerTest, TestInitializationCallbackIsCalled) {
  bool init_callback_was_called = false;
  base::OnceClosure closure = base::BindLambdaForTesting(
      [&init_callback_was_called]() { init_callback_was_called = true; });
  AccountManager account_manager;
  InitializeAccountManager(&account_manager, std::move(closure));
  ASSERT_TRUE(init_callback_was_called);
}

// Tests that |AccountManager::Initialize|'s callback parameter is called, if
// |AccountManager::Initialize| is invoked after Account Manager has been fully
// initialized.
TEST_F(AccountManagerTest,
       TestInitializationCallbackIsCalledIfAccountManagerIsAlreadyInitialized) {
  // Make sure that Account Manager is fully initialized.
  AccountManager account_manager;
  InitializeAccountManager(&account_manager, base::DoNothing());

  // Send a duplicate initialization call.
  bool init_callback_was_called = false;
  base::OnceClosure closure = base::BindLambdaForTesting(
      [&init_callback_was_called]() { init_callback_was_called = true; });
  InitializeAccountManager(&account_manager, std::move(closure));
  ASSERT_TRUE(init_callback_was_called);
}

TEST_F(AccountManagerTest, TestUpsert) {
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);

  std::vector<AccountManager::Account> accounts = GetAccountsBlocking();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(kGaiaAccountKey_, accounts[0].key);
  EXPECT_EQ(kRawUserEmail, accounts[0].raw_email);
}

TEST_F(AccountManagerTest, TestTokenPersistence) {
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();
  std::vector<AccountManager::Account> accounts = GetAccountsBlocking();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(kGaiaAccountKey_, accounts[0].key);
  EXPECT_EQ(kRawUserEmail, accounts[0].raw_email);
  EXPECT_EQ(kGaiaToken, account_manager_->accounts_[kGaiaAccountKey_].token);
}

TEST_F(AccountManagerTest, TestAccountEmailPersistence) {
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();
  const std::string raw_email = GetAccountEmailBlocking(kGaiaAccountKey_);
  EXPECT_EQ(kRawUserEmail, raw_email);
}

TEST_F(AccountManagerTest, UpdatingAccountEmailShouldNotOverwriteTokens) {
  const std::string new_email = "new-email@example.org";
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  account_manager_->UpdateEmail(kGaiaAccountKey_, new_email);
  task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();
  const std::string raw_email = GetAccountEmailBlocking(kGaiaAccountKey_);
  EXPECT_EQ(new_email, raw_email);
  EXPECT_EQ(kGaiaToken, account_manager_->accounts_[kGaiaAccountKey_].token);
}

TEST_F(AccountManagerTest, UpsertAccountCanUpdateEmail) {
  const std::string new_email = "new-email@example.org";
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  account_manager_->UpsertAccount(kGaiaAccountKey_, new_email, kGaiaToken);
  task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();
  const std::string raw_email = GetAccountEmailBlocking(kGaiaAccountKey_);
  EXPECT_EQ(new_email, raw_email);
}

TEST_F(AccountManagerTest, UpdatingTokensShouldNotOverwriteAccountEmail) {
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  account_manager_->UpdateToken(kGaiaAccountKey_, kNewGaiaToken);
  task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();
  const std::string raw_email = GetAccountEmailBlocking(kGaiaAccountKey_);
  EXPECT_EQ(kRawUserEmail, raw_email);
  EXPECT_EQ(kNewGaiaToken, account_manager_->accounts_[kGaiaAccountKey_].token);
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnTokenInsertion) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->AddObserver(observer.get());

  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer->is_token_upserted_callback_called_);
  EXPECT_EQ(1UL, observer->accounts_.size());
  EXPECT_EQ(kGaiaAccountKey_, *observer->accounts_.begin());
  EXPECT_EQ(kGaiaAccountKey_, observer->last_upserted_account_key_);
  EXPECT_EQ(kRawUserEmail, observer->last_upserted_account_email_);

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnTokenUpdate) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->AddObserver(observer.get());
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();

  // Observers should be called when token is updated.
  observer->is_token_upserted_callback_called_ = false;
  account_manager_->UpdateToken(kGaiaAccountKey_, kNewGaiaToken);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer->is_token_upserted_callback_called_);
  EXPECT_EQ(1UL, observer->accounts_.size());
  EXPECT_EQ(kGaiaAccountKey_, *observer->accounts_.begin());
  EXPECT_EQ(kGaiaAccountKey_, observer->last_upserted_account_key_);
  EXPECT_EQ(kRawUserEmail, observer->last_upserted_account_email_);

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, ObserversAreNotNotifiedIfTokenIsNotUpdated) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->AddObserver(observer.get());
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();

  // Observers should not be called when token is not updated.
  observer->is_token_upserted_callback_called_ = false;
  account_manager_->UpdateToken(kGaiaAccountKey_, kGaiaToken);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, RemovedAccountsAreImmediatelyUnavailable) {
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);

  account_manager_->RemoveAccount(kGaiaAccountKey_);
  EXPECT_TRUE(GetAccountsBlocking().empty());
}

TEST_F(AccountManagerTest, AccountsCanBeRemovedByRawEmail) {
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);

  account_manager_->RemoveAccount(kRawUserEmail);
  EXPECT_TRUE(GetAccountsBlocking().empty());
}

TEST_F(AccountManagerTest, AccountsCanBeRemovedByCanonicalEmail) {
  const std::string raw_email = "abc.123.456@gmail.com";
  const std::string canonical_email = "abc123456@gmail.com";

  account_manager_->UpsertAccount(kGaiaAccountKey_, raw_email, kGaiaToken);

  account_manager_->RemoveAccount(canonical_email);
  EXPECT_TRUE(GetAccountsBlocking().empty());
}

TEST_F(AccountManagerTest, AccountRemovalIsPersistedToDisk) {
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  account_manager_->RemoveAccount(kGaiaAccountKey_);
  task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();
  EXPECT_TRUE(GetAccountsBlocking().empty());
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnAccountRemoval) {
  auto observer = std::make_unique<AccountManagerObserver>();
  account_manager_->AddObserver(observer.get());
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(observer->is_account_removed_callback_called_);
  account_manager_->RemoveAccount(kGaiaAccountKey_);
  EXPECT_TRUE(observer->is_account_removed_callback_called_);
  EXPECT_TRUE(observer->accounts_.empty());
  EXPECT_EQ(kGaiaAccountKey_, observer->last_removed_account_key_);
  EXPECT_EQ(kRawUserEmail, observer->last_removed_account_email_);

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, TokenRevocationIsAttemptedForGaiaAccountRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(kGaiaToken));

  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();

  account_manager_->RemoveAccount(kGaiaAccountKey_);
}

TEST_F(AccountManagerTest,
       TokenRevocationIsNotAttemptedForNonGaiaAccountRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(_)).Times(0);

  account_manager_->UpsertAccount(kActiveDirectoryAccountKey_, kRawUserEmail,
                                  AccountManager::kActiveDirectoryDummyToken);
  task_environment_.RunUntilIdle();

  account_manager_->RemoveAccount(kActiveDirectoryAccountKey_);
}

TEST_F(AccountManagerTest,
       TokenRevocationIsNotAttemptedForInvalidTokenRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(_)).Times(0);

  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail,
                                  AccountManager::kInvalidToken);
  task_environment_.RunUntilIdle();

  account_manager_->RemoveAccount(kGaiaAccountKey_);
}

TEST_F(AccountManagerTest, OldTokenIsNotRevokedOnTokenUpdateByDefault) {
  ResetAndInitializeAccountManager();
  // Token should not be revoked.
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(kGaiaToken))
      .Times(0);
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);

  // Update the token.
  account_manager_->UpdateToken(kGaiaAccountKey_, kNewGaiaToken);
  task_environment_.RunUntilIdle();
}

TEST_F(AccountManagerTest, IsTokenAvailableReturnsTrueForValidGaiaAccounts) {
  EXPECT_FALSE(account_manager_->IsTokenAvailable(kGaiaAccountKey_));
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail, kGaiaToken);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(account_manager_->IsTokenAvailable(kGaiaAccountKey_));
}

TEST_F(AccountManagerTest,
       IsTokenAvailableReturnsFalseForActiveDirectoryAccounts) {
  EXPECT_FALSE(account_manager_->IsTokenAvailable(kActiveDirectoryAccountKey_));
  account_manager_->UpsertAccount(kActiveDirectoryAccountKey_, kRawUserEmail,
                                  AccountManager::kActiveDirectoryDummyToken);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(account_manager_->IsTokenAvailable(kActiveDirectoryAccountKey_));
  EXPECT_TRUE(
      IsAccountKeyPresent(GetAccountsBlocking(), kActiveDirectoryAccountKey_));
}

TEST_F(AccountManagerTest, IsTokenAvailableReturnsTrueForInvalidTokens) {
  EXPECT_FALSE(account_manager_->IsTokenAvailable(kGaiaAccountKey_));
  account_manager_->UpsertAccount(kGaiaAccountKey_, kRawUserEmail,
                                  AccountManager::kInvalidToken);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(account_manager_->IsTokenAvailable(kGaiaAccountKey_));
  EXPECT_TRUE(IsAccountKeyPresent(GetAccountsBlocking(), kGaiaAccountKey_));
}

}  // namespace chromeos
