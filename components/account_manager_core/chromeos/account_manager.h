// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/account_manager_core/account.h"

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;
class PrefRegistrySimple;
class PrefService;

namespace base {
class SequencedTaskRunner;
class ImportantFileWriter;
class TimeTicks;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}

namespace account_manager {

class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManager {
 public:
  // A dummy token stored against Active Directory accounts in Account Manager.
  // Accounts stored in Account Manager must have a token associated with them.
  // Active Directory accounts use Kerberos tickets for authentication, which is
  // handled by a different infrastructure. Hence, we need a dummy token to
  // store Active Directory accounts in Account Manager.
  // See |AccountManager::UpsertToken|.
  static const char kActiveDirectoryDummyToken[];

  // A special token that is guaranteed to cheaply fail all network requests
  // performed using it.
  // Note that it neither marks an account in Account Manager as invalid, nor
  // removes the account. This is useful in scenarios where account names are
  // imported from elsewhere (Chrome content area or ARC++) and their tokens are
  // not yet known, but at the same time, these accounts need to be surfaced on
  // the UI.
  // Do not use this token for Active Directory accounts,
  // |kActiveDirectoryDummyToken| is meant for that.
  // See |AccountManager::UpsertToken|.
  static const char* const kInvalidToken;

  // Callback used for the (asynchronous) GetAccounts() call.
  using AccountListCallback =
      base::OnceCallback<void(const std::vector<::account_manager::Account>&)>;

  using DelayNetworkCallRunner =
      base::RepeatingCallback<void(base::OnceClosure)>;

  class Observer {
   public:
    Observer();
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    virtual ~Observer();

    // Called when the token for |account| is updated/inserted.
    // Use |AccountManager::AddObserver| to add an |Observer|.
    // Note: |Observer|s which register with |AccountManager| before its
    // initialization is complete will get notified when |AccountManager| is
    // fully initialized.
    // Note: |Observer|s which register with |AccountManager| after its
    // initialization is complete will not get an immediate
    // notification-on-registration.
    virtual void OnTokenUpserted(const ::account_manager::Account& account) = 0;

    // Called when an account has been removed from AccountManager.
    // Observers that may have cached access tokens (fetched via
    // |AccountManager::CreateAccessTokenFetcher|), must clear their cache entry
    // for this |account| on receiving this callback.
    virtual void OnAccountRemoved(
        const ::account_manager::Account& account) = 0;
  };

  // Note: |Initialize| MUST be called at least once on this object.
  AccountManager();
  AccountManager(const AccountManager&) = delete;
  AccountManager& operator=(const AccountManager&) = delete;
  virtual ~AccountManager();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Sets a |PrefService|. Account Manager will use this instance to read its
  // policies.
  void SetPrefService(PrefService* pref_service);

  // |home_dir| is the path of the Device Account's home directory (root of the
  // user's cryptohome). If |home_dir| is |base::FilePath::empty()|, then |this|
  // |AccountManager| does not persist any data to disk.
  // |request_context| is a non-owning pointer.
  // |delay_network_call_runner| is basically a wrapper for
  // |ash::DelayNetworkCall|. Cannot use |ash::DelayNetworkCall| due
  // to linking/dependency constraints.
  // This method MUST be called at least once in the lifetime of AccountManager.
  void Initialize(
      const base::FilePath& home_dir,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DelayNetworkCallRunner delay_network_call_runner);

  // Same as above except that it accepts an |initialization_callback|, which
  // will be called after Account Manager has been fully initialized.
  // If Account Manager has already been fully initialized,
  // |initialization_callback| is called immediately.
  // Note: During initialization, there is no ordering guarantee between
  // |initialization_callback| and Account Manager's observers getting their
  // callbacks.
  void Initialize(
      const base::FilePath& home_dir,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DelayNetworkCallRunner delay_network_call_runner,
      base::OnceClosure initialization_callback);

  // Initializes |AccountManager| for ephemeral / in-memory usage.
  // Useful for tests that cannot afford to write to disk and clean up after
  // themselves.
  void InitializeInEphemeralMode(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Same as above, except it allows clients to provide a callback which will be
  // called after initialization.
  void InitializeInEphemeralMode(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::OnceClosure initialization_callback);

  // Returns |true| if |AccountManager| has been fully initialized.
  bool IsInitialized() const;

  // Gets (async) a list of account keys known to `AccountManager`. Note that
  // `callback` will be immediately called in the same thread if
  // `AccountManager` has been fully initialized and hence it may not be safe to
  // call this method directly in some class's constructor, with a callback on
  // the same class, since it may result in a method call on a partially
  // constructed object.
  void GetAccounts(AccountListCallback callback);

  // Gets (async) the raw, un-canonicalized email id corresponding to
  // `account_key`. `callback` is called with an empty string if `account_key`
  // is not known to Account Manager.
  void GetAccountEmail(const ::account_manager::AccountKey& account_key,
                       base::OnceCallback<void(const std::string&)> callback);

  // Removes an account. Does not do anything if `account_key` is not known by
  // `AccountManager`.
  // Observers are notified about an account removal through
  // `Observer::OnAccountRemoved`.
  // If the account being removed is a GAIA account, a token revocation with
  // GAIA is also attempted, on a best effort basis. Even if token revocation
  // with GAIA fails, AccountManager will forget the account.
  void RemoveAccount(const ::account_manager::AccountKey& account_key);

  // Updates or inserts an account. `raw_email` is the raw, un-canonicalized
  // email id for `account_key`. `raw_email` must not be empty. Use
  // `AccountManager::kActiveDirectoryDummyToken` as the `token` for Active
  // Directory accounts, and `AccountManager::kInvalidToken` for Gaia accounts
  // with unknown tokens.
  // Note: This API is idempotent.
  void UpsertAccount(const ::account_manager::AccountKey& account_key,
                     const std::string& raw_email,
                     const std::string& token);

  // Updates the token for the account corresponding to the given `account_key`.
  // The account must be known to Account Manager. See `UpsertAccount` for
  // information about adding an account.
  // Note: This API is idempotent.
  void UpdateToken(const ::account_manager::AccountKey& account_key,
                   const std::string& token);

  // Add a non owning pointer to an |AccountManager::Observer|.
  void AddObserver(Observer* observer);

  // Removes an |AccountManager::Observer|. Does nothing if the |observer| is
  // not in the list of known observers.
  void RemoveObserver(Observer* observer);

  // Sets the provided URL Loader Factory. Used only by tests.
  void SetUrlLoaderFactoryForTests(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Creates and returns an `OAuth2AccessTokenFetcher` using the refresh token
  // stored for `account_key`.
  // Virtual for testing.
  virtual std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const ::account_manager::AccountKey& account_key,
      OAuth2AccessTokenConsumer* consumer);

  // Returns |true| if an LST is available for |account_key|. Note that
  // "availability" does not guarantee "validity", i.e. this method will return
  // true for LSTs that have expired / been invalidated.
  // Note: Always returns false for Active Directory accounts.
  // Note: This method will return |false| if |AccountManager| has not been
  // initialized yet.
  bool IsTokenAvailable(const ::account_manager::AccountKey& account_key) const;

  // Calls the `callback` with true if the token stored against `account_key` is
  // a dummy Gaia token.
  void HasDummyGaiaToken(const ::account_manager::AccountKey& account_key,
                         base::OnceCallback<void(bool)> callback);

  // Calls the `callback` with a list of pairs of `account_key` and boolean
  // which is set to true if the token stored against `account_key` is a dummy
  // Gaia token, for all accounts stored in AccountManager. See
  // `HasDummyGaiaToken`.
  void CheckDummyGaiaTokenForAllAccounts(
      base::OnceCallback<
          void(const std::vector<std::pair<::account_manager::Account, bool>>&)>
          callback);

  // Returns the Base16 encoded SHA1 hash for the token stored against
  // `account_key`. This hash is meant only for debugging purposes and is not
  // meant be used as a cryptographically secure hash. Do not persist this
  // outside the user's cryptohome.
  // Returns an empty string if `account_key` is not available in
  // `AccountManager`.
  // TODO(http://b/297484217): Remove this API.
  void GetTokenHash(const ::account_manager::AccountKey& account_key,
                    base::OnceCallback<void(const std::string&)> callback);

  base::WeakPtr<AccountManager> GetWeakPtr();

 private:
  enum InitializationState {
    kNotStarted,   // Initialize has not been called
    kInProgress,   // Initialize has been called but not completed
    kInitialized,  // Initialization was successfully completed
  };

  // Account Manager's internal information about an account.
  struct AccountInfo {
    std::string raw_email;
    std::string token;
  };

  // A util class to revoke Gaia tokens on server. This class is meant to be
  // used for a single request.
  class GaiaTokenRevocationRequest;

  // A util class to fetch access tokens.
  class AccessTokenFetcher;

  friend class AccountManagerTest;
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest, TestInitializationCompletes);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest, TestTokenPersistence);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest,
                           UpdatingAccountEmailShouldNotOverwriteTokens);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest,
                           UpdatingTokensShouldNotOverwriteAccountEmail);

  using AccountMap = std::map<::account_manager::AccountKey, AccountInfo>;

  // Same as the public |Initialize| except for a |task_runner|.
  void Initialize(
      const base::FilePath& home_dir,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DelayNetworkCallRunner delay_network_call_runner,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::OnceClosure initialization_callback);

  // Loads accounts from disk and returns the result.
  static AccountMap LoadAccountsFromDisk(
      const base::FilePath& tokens_file_path);

  // Reads accounts from |accounts| and inserts them in |accounts_| and runs all
  // callbacks waiting on |AccountManager| initialization.
  // |initialization_start_time| is the time at which
  // |AccountManager::Initialize| was called.
  void InsertAccountsAndRunInitializationCallbacks(
      const base::TimeTicks& initialization_start_time,
      const AccountMap& accounts);

  // Accepts a closure and runs it immediately if |AccountManager| has already
  // been initialized, otherwise saves the |closure| for running later, when the
  // class is initialized.
  void RunOnInitialization(base::OnceClosure closure);

  // Does the actual work of upserting an account and performing related tasks
  // like revoking old tokens and informing observers. All account updates
  // funnel through to this method. Assumes that |AccountManager| initialization
  // (|init_state_|) is complete.
  void UpsertAccountInternal(const ::account_manager::AccountKey& account_key,
                             const AccountInfo& account);

  // Posts a task on |task_runner_|, which is usually a background thread, to
  // persist the current state of |accounts_|.
  void PersistAccountsAsync();

  // Gets a serialized representation of accounts.
  std::string GetSerializedAccounts();

  // Gets the publicly viewable information stored in `accounts_`.
  std::vector<::account_manager::Account> GetAccountsView();

  // Notifies |Observer|s about a token update for |account|.
  void NotifyTokenObservers(const ::account_manager::Account& account);

  // Notifies |Observer|s about an |account| removal.
  void NotifyAccountRemovalObservers(const ::account_manager::Account& account);

  // Revokes |account_key|'s token on the relevant backend.
  // Note: Does not do anything if the |account_manager::AccountType|
  // of |account_key| does not support server token revocation.
  // Note: |account_key| may or may not be present in |accounts_|. Call this
  // method *after* modifying or deleting old tokens from |accounts_|.
  void MaybeRevokeTokenOnServer(
      const ::account_manager::AccountKey& account_key,
      const std::string& old_token);

  // Revokes |refresh_token| with GAIA. Virtual for testing.
  virtual void RevokeGaiaTokenOnServer(const std::string& refresh_token);

  // Called by |GaiaTokenRevocationRequest| to notify its request completion.
  // Deletes |request| from |pending_token_revocation_requests_|, if present.
  void DeletePendingTokenRevocationRequest(GaiaTokenRevocationRequest* request);

  // Returns |true| if |AccountManager| is operating in ephemeral / in-memory
  // mode, and not persisting anything to disk.
  bool IsEphemeralMode() const;

  // Returns the refresh token for `account_key`, if present. `account_key` must
  // be a Gaia account. Assumes that `AccountManager` initialization
  // (`init_state_`) is complete.
  std::optional<std::string> GetRefreshToken(
      const ::account_manager::AccountKey& account_key);

  // Returns `url_loader_factory_`. Assumes that `AccountManager` initialization
  // (`init_state_`) is complete.
  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory();

  // Status of this object's initialization.
  InitializationState init_state_ = InitializationState::kNotStarted;

  // All tokens, if channel bound, are bound to |url_loader_factory_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // An indirect way to access `ash::DelayNetworkCall`. We cannot use
  // `ash::DelayNetworkCall` directly here due to linking/dependency
  // issues.
  DelayNetworkCallRunner delay_network_call_runner_;

  // Non-owning pointer.
  raw_ptr<PrefService, DanglingUntriaged> pref_service_ = nullptr;

  // A task runner for disk I/O.
  // Will be |nullptr| if |AccountManager| is operating in ephemeral mode.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Writes |AccountManager|'s state to disk.
  // Will be |nullptr| if |AccountManager| is operating in ephemeral mode.
  std::unique_ptr<base::ImportantFileWriter> writer_;

  // Cryptohome root.
  // Will be |base::FilePath::empty()| if |AccountManager| is operating in
  // ephemeral mode.
  base::FilePath home_dir_;

  // A map from |AccountKey|s to |AccountInfo|.
  AccountMap accounts_;

  // Callbacks waiting on class initialization (|init_state_|).
  std::vector<base::OnceClosure> initialization_callbacks_;

  // A list of |AccountManager| observers.
  // Verifies that the list is empty on destruction.
  base::ObserverList<Observer, true /* check_empty */>::Unchecked observers_;

  // A list of pending token revocation requests.
  // |AccountManager| is a long living object in general and these requests are
  // basically one shot fire-and-forget requests but for ASAN tests, we do not
  // want to have dangling pointers to pending requests.
  std::vector<std::unique_ptr<GaiaTokenRevocationRequest>>
      pending_token_revocation_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountManager> weak_factory_{this};
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_H_
