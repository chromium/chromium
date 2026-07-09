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
#include "mojo/public/cpp/bindings/remote.h"

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace account_manager {

class AccountManager;

// Implementation of |AccountManagerFacade| that talks to
// |account_manager::AccountManager|.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacadeImpl
    : public AccountManagerFacade,
      public AccountManager::Observer {
 public:
  // `account_manager` is the local AccountManager instance. It must be non-null
  // and outlive the constructed `AccountManagerFacadeImpl` instance.
  // `account_manager_remote` is a Mojo `Remote` to the account manager, used
  // for methods that have not yet been migrated to use `account_manager`.
  // `remote_version` is the Mojo API version of the remote.
  // `init_finished` is called after `this` has been fully initialized.
  //
  // TODO(b/365741912, b/365902693): Remove `init_finished`. Now that the
  // observer is registered in-process rather than over crosapi, the facade
  // finishes initializing before the constructor returns, so callers can just
  // run their setup right after constructing it.
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

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           HistogramsForZeroAccountManagerRemoteDisconnections);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           HistogramsForAccountManagerRemoteDisconnection);

  // A utility class to fetch access tokens over Mojo.
  class AccessTokenFetcher;

  // Proxy method to call `CreateAccessTokenFetcher` on
  // `account_manager_remote_`. Returns `true` if `account_manager_remote_` is
  // bound and the call was queued successfully.
  bool CreateAccessTokenFetcher(
      crosapi::mojom::AccountKeyPtr account_key,
      const std::string& oauth_consumer_name,
      crosapi::mojom::AccountManager::CreateAccessTokenFetcherCallback
          callback);

  // Runs `closure` if/when `account_manager_remote_` gets disconnected.
  void RunOnAccountManagerRemoteDisconnection(base::OnceClosure closure);

  // Mojo disconnect handler for `account_manager_remote_`.
  void OnAccountManagerRemoteDisconnected();

  void FlushMojoForTesting();

  // Mojo API version on the remote (Ash) side.
  const uint32_t remote_version_;

  // Number of Mojo pipe disconnections seen by `account_manager_remote_`.
  int num_remote_disconnections_ = 0;

  std::vector<base::OnceClosure> account_manager_remote_disconnection_handlers_;

  mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote_;

  base::ObserverList<AccountManagerFacade::Observer> observer_list_;

  const raw_ref<AccountManager> account_manager_;
  base::ScopedObservation<AccountManager, AccountManager::Observer>
      account_manager_observation_{this};

  base::WeakPtrFactory<AccountManagerFacadeImpl> weak_factory_{this};
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
