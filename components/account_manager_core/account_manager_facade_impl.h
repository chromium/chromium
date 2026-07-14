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
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager.h"

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
  // `init_finished` is called after `this` has been fully initialized.
  //
  // TODO(b/365741912, b/365902693): Remove `init_finished`. Now that the
  // observer is registered in-process rather than over crosapi, the facade
  // finishes initializing before the constructor returns, so callers can just
  // run their setup right after constructing it.
  AccountManagerFacadeImpl(
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
  base::ObserverList<AccountManagerFacade::Observer> observer_list_;
  const raw_ref<AccountManager> account_manager_;
  base::ScopedObservation<AccountManager, AccountManager::Observer>
      account_manager_observation_{this};
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
