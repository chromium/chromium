// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace account_manager {

class AccountManager;

// Implementation of |AccountManagerFacade| that talks to
// |account_manager::AccountManager|.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacadeImpl
    : public AccountManagerFacade,
      public crosapi::mojom::AccountManagerObserver,
      public AccountManager::Observer {
 public:
  // `account_manager` is the local AccountManager instance. It must be non-null
  // and outlive the constructed `AccountManagerFacadeImpl` instance.
  // `account_manager_remote` is a Mojo `Remote` to the account manager, used
  // for methods that have not yet been migrated to use `account_manager`.
  // `remote_version` is the Mojo API version of the remote.
  // `init_finished` is called after `this` has been fully initialized.
  AccountManagerFacadeImpl(
      mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
      uint32_t remote_version,
      AccountManager* account_manager,
      base::OnceClosure init_finished = base::DoNothing());
  AccountManagerFacadeImpl(const AccountManagerFacadeImpl&) = delete;
  AccountManagerFacadeImpl& operator=(const AccountManagerFacadeImpl&) = delete;
  ~AccountManagerFacadeImpl() override;

  // AccountManagerFacade overrides:
  void AddObserver(AccountManagerFacade::Observer* observer) override;
  void RemoveObserver(AccountManagerFacade::Observer* observer) override;
  void GetAccounts(
      base::OnceCallback<void(const std::vector<Account>&)> callback) override;
  void GetPersistentErrorForAccount(
      const AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback)
      override;
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const AccountKey& account,
      OAuth2AccessTokenConsumer* consumer) override;
  void ReportAuthError(const account_manager::AccountKey& account,
                       const GoogleServiceAuthError& error) override;
  void UpsertAccountForTesting(const Account& account,
                               const std::string& token_value) override;
  void RemoveAccountForTesting(const AccountKey& account) override;

  // AccountManager::Observer overrides:
  void OnTokenUpserted(const Account& account) override;
  void OnAccountRemoved(const Account& account) override;

  // crosapi::mojom::AccountManagerObserver overrides:
  void OnSigninDialogClosed() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           InitializationStatusIsCorrectlySet);
  FRIEND_TEST_ALL_PREFIXES(
      AccountManagerFacadeImplTest,
      AccessTokenFetcherCanBeCreatedBeforeAccountManagerFacadeInitialization);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           HistogramsForZeroAccountManagerRemoteDisconnections);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           HistogramsForAccountManagerRemoteDisconnection);
  FRIEND_TEST_ALL_PREFIXES(
      AccountManagerFacadeImplTest,
      HistogramsForZeroAccountManagerObserverReceiverDisconnections);
  FRIEND_TEST_ALL_PREFIXES(
      AccountManagerFacadeImplTest,
      HistogramsForAccountManagerObserverReceiverDisconnections);

  // A utility class to fetch access tokens over Mojo.
  class AccessTokenFetcher;

  void OnReceiverReceived(
      mojo::PendingReceiver<AccountManagerObserver> receiver);

  void GetPersistentErrorInternal(
      const AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback);

  // Proxy method to call `CreateAccessTokenFetcher` on
  // `account_manager_remote_`. Returns `true` if `account_manager_remote_` is
  // bound and the call was queued successfully.
  bool CreateAccessTokenFetcher(
      crosapi::mojom::AccountKeyPtr account_key,
      const std::string& oauth_consumer_name,
      crosapi::mojom::AccountManager::CreateAccessTokenFetcherCallback
          callback);

  // The initialization sequence for `AccountManagerFacadeImpl` consists of
  // adding an observer to the remote.
  //
  // Remote-querying methods won't actually produce a remote call until the
  // initialization sequence is finished (instead, they will be queued in
  // `initialization_callbacks_`).
  //
  // `FinishInitSequenceIfNotAlreadyFinished` invokes callbacks from
  // `initialization_callbacks_` and marks the initialization as finished.
  void FinishInitSequenceIfNotAlreadyFinished();
  // `closure` will be invoked after the initialization sequence is finished.
  // See `FinishInitSequenceIfNotAlreadyFinished` for details.
  void RunAfterInitializationSequence(base::OnceClosure closure);

  // Runs `closure` if/when `account_manager_remote_` gets disconnected.
  void RunOnAccountManagerRemoteDisconnection(base::OnceClosure closure);

  // Mojo disconnect handler for `account_manager_remote_`.
  void OnAccountManagerRemoteDisconnected();

  // Mojo disconnect handler for `receiver_`.
  void OnAccountManagerObserverReceiverDisconnected();

  bool IsInitialized();

  void FlushMojoForTesting();

  // Mojo API version on the remote (Ash) side.
  const uint32_t remote_version_;

  // Number of Mojo pipe disconnections seen by `account_manager_remote_`.
  int num_remote_disconnections_ = 0;

  // Number of Mojo pipe disconnections seen by `receiver_`.
  int num_receiver_disconnections_ = 0;

  bool is_initialized_ = false;
  std::vector<base::OnceClosure> initialization_callbacks_;
  std::vector<base::OnceClosure> account_manager_remote_disconnection_handlers_;

  mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote_;
  std::unique_ptr<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>
      receiver_;

  base::ObserverList<AccountManagerFacade::Observer> observer_list_;

  const raw_ref<AccountManager> account_manager_;
  base::ScopedObservation<AccountManager, AccountManager::Observer>
      account_manager_observation_{this};

  base::WeakPtrFactory<AccountManagerFacadeImpl> weak_factory_{this};
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
