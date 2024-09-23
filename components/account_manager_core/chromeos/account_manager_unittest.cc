// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/gaia_urls.h"
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
using ::testing::Property;

constexpr char kGaiaToken[] = "gaia_token";
constexpr char kNewGaiaToken[] = "new_gaia_token";
constexpr char kRawUserEmail[] = "user@example.com";
constexpr char kFakeClientId[] = "fake-client-id";
constexpr char kFakeClientSecret[] = "fake-client-secret";
constexpr char kFakeAccessToken[] = "fake-access-token";
// Same access token value as above in `kFakeAccessToken`.
constexpr char kAccessTokenResponse[] = R"(
    {
      "access_token": "fake-access-token",
      "expires_in": 3600,
      "token_type": "Bearer",
      "id_token": "id_token"
    })";
const ::account_manager::AccountKey kGaiaAccountKey = {
    "gaia_id", ::account_manager::AccountType::kGaia};
const ::account_manager::AccountKey kActiveDirectoryAccountKey = {
    "object_guid", ::account_manager::AccountType::kActiveDirectory};

bool IsAccountKeyPresent(
    const std::vector<::account_manager::Account>& accounts,
    const ::account_manager::AccountKey& account_key) {
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
  AccountManagerSpy(const AccountManagerSpy&) = delete;
  AccountManagerSpy& operator=(const AccountManagerSpy&) = delete;
  ~AccountManagerSpy() override = default;

  MOCK_METHOD(void, RevokeGaiaTokenOnServer, (const std::string&));
};

class AccountManagerTest : public testing::Test {
 public:
  AccountManagerTest() = default;
  AccountManagerTest(const AccountManagerTest&) = delete;
  AccountManagerTest& operator=(const AccountManagerTest&) = delete;
  ~AccountManagerTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    AccountManager::RegisterPrefs(pref_service_.registry());
    ResetAndInitializeAccountManager();
  }

  // Gets the list of accounts stored in |account_manager_|.
  std::vector<::account_manager::Account> GetAccountsBlocking() {
    return GetAccountsBlocking(account_manager_.get());
  }

  // Gets the list of accounts stored in |account_manager|.
  std::vector<::account_manager::Account> GetAccountsBlocking(
      AccountManager* const account_manager) {
    base::test::TestFuture<const std::vector<::account_manager::Account>&>
        future;
    account_manager->GetAccounts(future.GetCallback());
    return future.Get();
  }

  // Gets the raw email for |account_key|.
  std::string GetAccountEmailBlocking(
      const ::account_manager::AccountKey& account_key) {
    base::test::TestFuture<const std::string&> future;
    account_manager_->GetAccountEmail(account_key, future.GetCallback());
    return future.Get();
  }

  bool HasDummyGaiaTokenBlocking(
      const ::account_manager::AccountKey& account_key) {
    base::test::TestFuture<bool> future;
    account_manager_->HasDummyGaiaToken(account_key, future.GetCallback());
    return future.Get();
  }

  // Helper method to reset and initialize |account_manager_| with default
  // parameters.
  void ResetAndInitializeAccountManager() {
    account_manager_ = std::make_unique<AccountManagerSpy>();
    InitializeAccountManager(account_manager_.get());
  }

  // |account_manager| is a non-owning pointer.
  void InitializeAccountManager(AccountManager* account_manager) {
    InitializeAccountManager(account_manager, tmp_dir_.GetPath());
  }

  // |account_manager| is a non-owning pointer.
  // |home_dir| is the cryptohome root.
  void InitializeAccountManager(AccountManager* account_manager,
                                const base::FilePath& home_dir) {
    InitializeAccountManager(account_manager, home_dir,
                             /* initialization_callback= */ base::DoNothing());
    RunAllPendingTasks();
    EXPECT_EQ(account_manager->init_state_,
              AccountManager::InitializationState::kInitialized);
    EXPECT_TRUE(account_manager->IsInitialized());
  }

  // |account_manager| is a non-owning pointer.
  // |initialization_callback| will be called after initialization is complete
  // (when |RunAllPendingTasks();| is called).
  void InitializeAccountManagerAsync(
      AccountManager* account_manager,
      base::OnceClosure initialization_callback) {
    InitializeAccountManager(account_manager,
                             /* home_dir= */ tmp_dir_.GetPath(),
                             std::move(initialization_callback));
  }

  // |account_manager| is a non-owning pointer.
  void InitializeEphemeralAccountManager(AccountManager* account_manager) {
    account_manager->InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper());
    account_manager->SetPrefService(&pref_service_);
    RunAllPendingTasks();
    EXPECT_TRUE(account_manager->IsInitialized());
  }

  void AddFakeAccessTokenResponse() {
    GURL url(GaiaUrls::GetInstance()->oauth2_token_url());
    test_url_loader_factory_.AddResponse(url.spec(), kAccessTokenResponse,
                                         net::HTTP_OK);
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

  // Returns an unowned pointer to |AccountManager|.
  AccountManager* account_manager() const { return account_manager_.get(); }

  // Returns an unowned pointer to |AccountManagerSpy|. Useful only for checking
  // expectations on the mock.
  AccountManagerSpy* account_manager_spy() const {
    return account_manager_.get();
  }

  scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

 private:
  void InitializeAccountManager(AccountManager* account_manager,
                                const base::FilePath& home_dir,
                                base::OnceClosure initialization_callback) {
    account_manager->Initialize(
        home_dir, test_url_loader_factory_.GetSafeWeakWrapper(),
        /* delay_network_call_runner= */
        base::BindRepeating([](base::OnceClosure closure) -> void {
          std::move(closure).Run();
        }),
        base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(initialization_callback));
    account_manager->SetPrefService(&pref_service_);
  }

  // Check base/test/task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir tmp_dir_;
  TestingPrefServiceSimple pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<AccountManagerSpy> account_manager_;
};

class AccountManagerObserver : public AccountManager::Observer {
 public:
  AccountManagerObserver() = default;
  AccountManagerObserver(const AccountManagerObserver&) = delete;
  AccountManagerObserver& operator=(const AccountManagerObserver&) = delete;
  ~AccountManagerObserver() override = default;

  void OnTokenUpserted(const ::account_manager::Account& account) override {
    is_token_upserted_callback_called_ = true;
    accounts_.insert(account.key);
    last_upserted_account_key_ = account.key;
    last_upserted_account_email_ = account.raw_email;
  }

  void OnAccountRemoved(const ::account_manager::Account& account) override {
    is_account_removed_callback_called_ = true;
    accounts_.erase(account.key);
    last_removed_account_key_ = account.key;
    last_removed_account_email_ = account.raw_email;
  }

  void Reset() {
    is_token_upserted_callback_called_ = false;
    is_account_removed_callback_called_ = false;
    last_upserted_account_key_ = std::nullopt;
    last_upserted_account_email_.clear();
    last_removed_account_key_ = std::nullopt;
    last_removed_account_email_.clear();
    accounts_.clear();
  }

  bool is_token_upserted_callback_called() const {
    return is_token_upserted_callback_called_;
  }

  bool is_account_removed_callback_called() const {
    return is_account_removed_callback_called_;
  }

  const ::account_manager::AccountKey& last_upserted_account_key() const {
    return last_upserted_account_key_.value();
  }

  const std::string& last_upserted_account_email() const {
    return last_upserted_account_email_;
  }

  const ::account_manager::AccountKey& last_removed_account_key() const {
    return last_removed_account_key_.value();
  }

  const std::string& last_removed_account_email() const {
    return last_removed_account_email_;
  }

  const std::set<::account_manager::AccountKey>& accounts() const {
    return accounts_;
  }

 private:
  bool is_token_upserted_callback_called_ = false;
  bool is_account_removed_callback_called_ = false;
  std::optional<::account_manager::AccountKey> last_upserted_account_key_;
  std::string last_upserted_account_email_;
  std::optional<::account_manager::AccountKey> last_removed_account_key_;
  std::string last_removed_account_email_;
  std::set<::account_manager::AccountKey> accounts_;
};

class MockAccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockAccessTokenConsumer() = default;
  MockAccessTokenConsumer(const MockAccessTokenConsumer&) = delete;
  MockAccessTokenConsumer& operator=(const MockAccessTokenConsumer&) = delete;
  ~MockAccessTokenConsumer() override = default;

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
    return "account_manager_unittest";
  }
};

TEST_F(AccountManagerTest, TestInitializationCompletes) {
  AccountManager account_manager;

  EXPECT_EQ(account_manager.init_state_,
            AccountManager::InitializationState::kNotStarted);
  // Test assertions will be made inside the method.
  InitializeAccountManager(&account_manager);
}

TEST_F(AccountManagerTest, TestInitializationCallbackIsCalled) {
  bool init_callback_was_called = false;
  base::OnceClosure closure = base::BindLambdaForTesting(
      [&init_callback_was_called]() { init_callback_was_called = true; });
  AccountManager account_manager;
  InitializeAccountManagerAsync(&account_manager, std::move(closure));
  RunAllPendingTasks();
  ASSERT_TRUE(init_callback_was_called);
}

// Tests that |AccountManager::Initialize|'s callback parameter is called, if
// |AccountManager::Initialize| is called twice.
TEST_F(AccountManagerTest,
       TestInitializationCallbackIsCalledIfAccountManagerIsAlreadyInitialized) {
  AccountManager account_manager;
  InitializeAccountManagerAsync(
      &account_manager, /* initialization_callback= */ base::DoNothing());
  EXPECT_FALSE(account_manager.IsInitialized());

  // Send a duplicate initialization call.
  bool init_callback_was_called = false;
  base::OnceClosure closure = base::BindLambdaForTesting(
      [&init_callback_was_called]() { init_callback_was_called = true; });
  InitializeAccountManagerAsync(&account_manager, std::move(closure));

  // Let initialization continue.
  EXPECT_FALSE(account_manager.IsInitialized());
  EXPECT_FALSE(init_callback_was_called);
  RunAllPendingTasks();
  EXPECT_TRUE(account_manager.IsInitialized());
  EXPECT_TRUE(init_callback_was_called);
}

TEST_F(AccountManagerTest, TestUpsert) {
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);

  std::vector<::account_manager::Account> accounts = GetAccountsBlocking();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(kGaiaAccountKey, accounts[0].key);
  EXPECT_EQ(kRawUserEmail, accounts[0].raw_email);
}

// Test that |AccountManager| saves its tokens to disk.
TEST_F(AccountManagerTest, TestTokenPersistence) {
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  ResetAndInitializeAccountManager();
  std::vector<::account_manager::Account> accounts = GetAccountsBlocking();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(kGaiaAccountKey, accounts[0].key);
  EXPECT_EQ(kRawUserEmail, accounts[0].raw_email);
  EXPECT_EQ(kGaiaToken, account_manager()->accounts_[kGaiaAccountKey].token);
}

// Test that |AccountManager| does not save its tokens to disk if an empty
// cryptohome root path is provided during initialization.
TEST_F(AccountManagerTest, TestTokenTransience) {
  const base::FilePath home_dir;

  {
    // Create a scoped |AccountManager|.
    AccountManager account_manager;
    InitializeAccountManager(&account_manager, home_dir);
    account_manager.UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
    RunAllPendingTasks();
  }

  // Create another |AccountManager| at the same path.
  AccountManager account_manager;
  InitializeAccountManager(&account_manager, home_dir);

  std::vector<::account_manager::Account> accounts =
      GetAccountsBlocking(&account_manager);
  EXPECT_EQ(0UL, accounts.size());
}

TEST_F(AccountManagerTest, TestEphemeralMode) {
  {
    // Create a scoped |AccountManager|.
    AccountManager account_manager;
    InitializeEphemeralAccountManager(&account_manager);
    account_manager.UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
    RunAllPendingTasks();
  }

  // Create another |AccountManager|.
  AccountManager account_manager;
  InitializeEphemeralAccountManager(&account_manager);

  std::vector<::account_manager::Account> accounts =
      GetAccountsBlocking(&account_manager);
  EXPECT_EQ(0UL, accounts.size());
}

TEST_F(AccountManagerTest, TestEphemeralModeInitializationCallback) {
  base::test::TestFuture<void> future;
  AccountManager account_manager;
  account_manager.InitializeInEphemeralMode(test_url_loader_factory(),
                                            future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(AccountManagerTest, TestAccountEmailPersistence) {
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  ResetAndInitializeAccountManager();
  const std::string raw_email = GetAccountEmailBlocking(kGaiaAccountKey);
  EXPECT_EQ(kRawUserEmail, raw_email);
}

TEST_F(AccountManagerTest, UpsertAccountCanUpdateEmail) {
  const std::string new_email = "new-email@example.org";
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  account_manager()->UpsertAccount(kGaiaAccountKey, new_email, kGaiaToken);
  RunAllPendingTasks();

  ResetAndInitializeAccountManager();
  const std::string raw_email = GetAccountEmailBlocking(kGaiaAccountKey);
  EXPECT_EQ(new_email, raw_email);
}

TEST_F(AccountManagerTest, UpdatingTokensShouldNotOverwriteAccountEmail) {
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  account_manager()->UpdateToken(kGaiaAccountKey, kNewGaiaToken);
  RunAllPendingTasks();

  ResetAndInitializeAccountManager();
  const std::string raw_email = GetAccountEmailBlocking(kGaiaAccountKey);
  EXPECT_EQ(kRawUserEmail, raw_email);
  EXPECT_EQ(kNewGaiaToken, account_manager()->accounts_[kGaiaAccountKey].token);
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnTokenInsertion) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called());

  account_manager()->AddObserver(observer.get());

  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();
  EXPECT_TRUE(observer->is_token_upserted_callback_called());
  EXPECT_EQ(1UL, observer->accounts().size());
  EXPECT_EQ(kGaiaAccountKey, *observer->accounts().begin());
  EXPECT_EQ(kGaiaAccountKey, observer->last_upserted_account_key());
  EXPECT_EQ(kRawUserEmail, observer->last_upserted_account_email());

  account_manager()->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnTokenUpdate) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called());

  account_manager()->AddObserver(observer.get());
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  // Observers should be called when token is updated.
  observer->Reset();
  account_manager()->UpdateToken(kGaiaAccountKey, kNewGaiaToken);
  RunAllPendingTasks();
  EXPECT_TRUE(observer->is_token_upserted_callback_called());
  EXPECT_EQ(1UL, observer->accounts().size());
  EXPECT_EQ(kGaiaAccountKey, *observer->accounts().begin());
  EXPECT_EQ(kGaiaAccountKey, observer->last_upserted_account_key());
  EXPECT_EQ(kRawUserEmail, observer->last_upserted_account_email());

  account_manager()->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, ObserversAreNotNotifiedIfTokenIsNotUpdated) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called());

  account_manager()->AddObserver(observer.get());
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  // Observers should not be called when token is not updated.
  observer->Reset();
  account_manager()->UpdateToken(kGaiaAccountKey, kGaiaToken);
  RunAllPendingTasks();
  EXPECT_FALSE(observer->is_token_upserted_callback_called());

  account_manager()->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, RemovedAccountsAreImmediatelyUnavailable) {
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);

  account_manager()->RemoveAccount(kGaiaAccountKey);
  EXPECT_TRUE(GetAccountsBlocking().empty());
}

TEST_F(AccountManagerTest, AccountRemovalIsPersistedToDisk) {
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  account_manager()->RemoveAccount(kGaiaAccountKey);
  RunAllPendingTasks();

  ResetAndInitializeAccountManager();
  EXPECT_TRUE(GetAccountsBlocking().empty());
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnAccountRemoval) {
  auto observer = std::make_unique<AccountManagerObserver>();
  account_manager()->AddObserver(observer.get());
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  EXPECT_FALSE(observer->is_account_removed_callback_called());
  account_manager()->RemoveAccount(kGaiaAccountKey);
  EXPECT_TRUE(observer->is_account_removed_callback_called());
  EXPECT_TRUE(observer->accounts().empty());
  EXPECT_EQ(kGaiaAccountKey, observer->last_removed_account_key());
  EXPECT_EQ(kRawUserEmail, observer->last_removed_account_email());

  account_manager()->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, TokenRevocationIsAttemptedForGaiaAccountRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_spy(), RevokeGaiaTokenOnServer(kGaiaToken));

  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  account_manager()->RemoveAccount(kGaiaAccountKey);
}

TEST_F(AccountManagerTest,
       TokenRevocationIsNotAttemptedForNonGaiaAccountRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_spy(), RevokeGaiaTokenOnServer(_)).Times(0);

  account_manager()->UpsertAccount(kActiveDirectoryAccountKey, kRawUserEmail,
                                   AccountManager::kActiveDirectoryDummyToken);
  RunAllPendingTasks();

  account_manager()->RemoveAccount(kActiveDirectoryAccountKey);
}

TEST_F(AccountManagerTest,
       TokenRevocationIsNotAttemptedForInvalidTokenRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_spy(), RevokeGaiaTokenOnServer(_)).Times(0);

  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail,
                                   AccountManager::kInvalidToken);
  RunAllPendingTasks();

  account_manager()->RemoveAccount(kGaiaAccountKey);
}

TEST_F(AccountManagerTest, OldTokenIsNotRevokedOnTokenUpdateByDefault) {
  ResetAndInitializeAccountManager();
  // Token should not be revoked.
  EXPECT_CALL(*account_manager_spy(), RevokeGaiaTokenOnServer(kGaiaToken))
      .Times(0);
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);

  // Update the token.
  account_manager()->UpdateToken(kGaiaAccountKey, kNewGaiaToken);
  RunAllPendingTasks();
}

TEST_F(AccountManagerTest, IsTokenAvailableReturnsTrueForValidGaiaAccounts) {
  EXPECT_FALSE(account_manager()->IsTokenAvailable(kGaiaAccountKey));
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();
  EXPECT_TRUE(account_manager()->IsTokenAvailable(kGaiaAccountKey));
}

TEST_F(AccountManagerTest,
       IsTokenAvailableReturnsFalseForActiveDirectoryAccounts) {
  EXPECT_FALSE(account_manager()->IsTokenAvailable(kActiveDirectoryAccountKey));
  account_manager()->UpsertAccount(kActiveDirectoryAccountKey, kRawUserEmail,
                                   AccountManager::kActiveDirectoryDummyToken);
  RunAllPendingTasks();
  EXPECT_FALSE(account_manager()->IsTokenAvailable(kActiveDirectoryAccountKey));
  EXPECT_TRUE(
      IsAccountKeyPresent(GetAccountsBlocking(), kActiveDirectoryAccountKey));
}

TEST_F(AccountManagerTest, IsTokenAvailableReturnsTrueForInvalidTokens) {
  EXPECT_FALSE(account_manager()->IsTokenAvailable(kGaiaAccountKey));
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail,
                                   AccountManager::kInvalidToken);
  RunAllPendingTasks();
  EXPECT_TRUE(account_manager()->IsTokenAvailable(kGaiaAccountKey));
  EXPECT_TRUE(IsAccountKeyPresent(GetAccountsBlocking(), kGaiaAccountKey));
}

TEST_F(AccountManagerTest, HasDummyGaiaTokenReturnsTrueForInvalidTokens) {
  EXPECT_FALSE(account_manager()->IsTokenAvailable(kGaiaAccountKey));
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail,
                                   AccountManager::kInvalidToken);
  RunAllPendingTasks();
  EXPECT_TRUE(HasDummyGaiaTokenBlocking(kGaiaAccountKey));
}

TEST_F(AccountManagerTest, HasDummyGaiaTokenReturnsFalseForValidTokens) {
  EXPECT_FALSE(account_manager()->IsTokenAvailable(kGaiaAccountKey));
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();
  EXPECT_FALSE(HasDummyGaiaTokenBlocking(kGaiaAccountKey));
}

TEST_F(AccountManagerTest, GetTokenHashReturnsAnEmptyStringForUnknownAccounts) {
  base::test::TestFuture<const std::string&> future;
  account_manager()->GetTokenHash(kGaiaAccountKey, future.GetCallback());
  EXPECT_EQ(std::string(), future.Get());
}

TEST_F(AccountManagerTest, GetTokenHashReturnsSha1Hash) {
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  base::test::TestFuture<const std::string&> future;
  account_manager()->GetTokenHash(kGaiaAccountKey, future.GetCallback());

  const base::SHA1Digest token_hash =
      base::SHA1Hash(base::as_byte_span(std::string(kGaiaToken)));
  const std::string token_hash_digest = base::HexEncode(token_hash);
  EXPECT_EQ(token_hash_digest, future.Get());
}

TEST_F(AccountManagerTest,
       AccessTokenFetcherCanBeCreatedBeforeAccountManagerInitialization) {
  {
    // Persist a token for kGaiaAccountKey.
    AccountManager account_manager;
    InitializeAccountManager(&account_manager);
    account_manager.UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
    RunAllPendingTasks();
  }

  AddFakeAccessTokenResponse();
  MockAccessTokenConsumer consumer;
  // Create an instance of `AccountManager` but do not initialize it yet.
  AccountManager account_manager;
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager.CreateAccessTokenFetcher(kGaiaAccountKey, &consumer);
  ASSERT_TRUE(access_token_fetcher != nullptr);

  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  EXPECT_CALL(consumer,
              OnGetTokenSuccess(
                  Field(&OAuth2AccessTokenConsumer::TokenResponse::access_token,
                        Eq(kFakeAccessToken))));
  InitializeAccountManager(&account_manager);
  RunAllPendingTasks();
  EXPECT_TRUE(account_manager.IsInitialized());
}

TEST_F(AccountManagerTest, AccessTokenFetchSucceedsForGaiaAccounts) {
  ResetAndInitializeAccountManager();
  account_manager()->UpsertAccount(kGaiaAccountKey, kRawUserEmail, kGaiaToken);
  RunAllPendingTasks();

  AddFakeAccessTokenResponse();
  MockAccessTokenConsumer consumer;
  EXPECT_CALL(consumer,
              OnGetTokenSuccess(
                  Field(&OAuth2AccessTokenConsumer::TokenResponse::access_token,
                        Eq(kFakeAccessToken))));
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager()->CreateAccessTokenFetcher(kGaiaAccountKey, &consumer);
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  RunAllPendingTasks();
}

TEST_F(AccountManagerTest, AccessTokenFetchFailsForActiveDirectoryAccounts) {
  ResetAndInitializeAccountManager();
  account_manager()->UpsertAccount(kActiveDirectoryAccountKey, kRawUserEmail,
                                   AccountManager::kActiveDirectoryDummyToken);
  RunAllPendingTasks();

  MockAccessTokenConsumer consumer;
  EXPECT_CALL(consumer,
              OnGetTokenFailure(Property(
                  &GoogleServiceAuthError::state,
                  Eq(GoogleServiceAuthError::State::USER_NOT_SIGNED_UP))));

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager()->CreateAccessTokenFetcher(kActiveDirectoryAccountKey,
                                                  &consumer);
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  RunAllPendingTasks();
}

TEST_F(AccountManagerTest, AccessTokenFetchFailsForUnknownAccounts) {
  ResetAndInitializeAccountManager();

  MockAccessTokenConsumer consumer;
  EXPECT_CALL(consumer,
              OnGetTokenFailure(Property(
                  &GoogleServiceAuthError::state,
                  Eq(GoogleServiceAuthError::State::USER_NOT_SIGNED_UP))));

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      account_manager()->CreateAccessTokenFetcher(kGaiaAccountKey, &consumer);
  access_token_fetcher->Start(kFakeClientId, kFakeClientSecret, /*scopes=*/{});
  RunAllPendingTasks();
}

}  // namespace account_manager
