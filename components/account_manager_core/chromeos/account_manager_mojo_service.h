// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_MOJO_SERVICE_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_MOJO_SERVICE_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/chromeos/access_token_fetcher.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
class SigninHelper;
}

namespace crosapi {

// Implements the |crosapi::mojom::AccountManager| interface in ash-chrome.
// It enables lacros-chrome to interact with accounts stored in the Chrome OS
// Account Manager.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerMojoService
    : public mojom::AccountManager,
      public account_manager::AccountManager::Observer {
 public:
  explicit AccountManagerMojoService(
      account_manager::AccountManager* account_manager);
  AccountManagerMojoService(const AccountManagerMojoService&) = delete;
  AccountManagerMojoService& operator=(const AccountManagerMojoService&) =
      delete;
  ~AccountManagerMojoService() override;

  void BindReceiver(mojo::PendingReceiver<mojom::AccountManager> receiver);

  void SetAccountManagerUI(
      std::unique_ptr<account_manager::AccountManagerUI> account_manager_ui);

  void OnAccountUpsertionFinishedForTesting(
      const account_manager::AccountUpsertionResult& result);

  // crosapi::mojom::AccountManager:
  void IsInitialized(IsInitializedCallback callback) override;
  void AddObserver(AddObserverCallback callback) override;
  void GetAccounts(GetAccountsCallback callback) override;
  void GetPersistentErrorForAccount(
      mojom::AccountKeyPtr mojo_account_key,
      GetPersistentErrorForAccountCallback callback) override;
  void ShowAddAccountDialog(mojom::AccountAdditionOptionsPtr options,
                            ShowAddAccountDialogCallback callback) override;
  void ShowReauthAccountDialog(
      const std::string& email,
      ShowReauthAccountDialogCallback callback) override;
  void ShowManageAccountsSettings() override;
  void CreateAccessTokenFetcher(
      mojom::AccountKeyPtr mojo_account_key,
      const std::string& oauth_consumer_name,
      CreateAccessTokenFetcherCallback callback) override;
  void ReportAuthError(mojom::AccountKeyPtr account,
                       mojom::GoogleServiceAuthErrorPtr error) override;

  // account_manager::AccountManager::Observer:
  void OnTokenUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;

 private:
  friend class AccountManagerMojoServiceTest;
  friend class TestAccountManagerObserver;
  friend class AccountManagerFacadeAshTest;
  friend class ash::SigninHelper;

  // This method is called by `ash::SigninHelper` which passes `AccountKey`
  // of account that was added.
  void OnAccountUpsertionFinished(
      const account_manager::AccountUpsertionResult& result);
  // A callback for `AccountManagerUI::ShowAccountAdditionDialog`.
  void OnSigninDialogClosed();
  void FinishUpsertAccount(
      const account_manager::AccountUpsertionResult& result);
  // Deletes `request` from `pending_access_token_requests_`, if present.
  void DeletePendingAccessTokenFetchRequest(AccessTokenFetcher* request);

  // Notifies observers about a change in the error status of `account_key`.
  // Does nothing if `account_key` does not correspond to any account in
  // `known_accounts`.
  void MaybeNotifyAuthErrorObservers(
      const account_manager::AccountKey& account_key,
      const GoogleServiceAuthError& error,
      const std::vector<account_manager::Account>& known_accounts);

  // Notifies observers that the account addition / re-authentication dialog was
  // closed (either successfully, or the user cancelled the flow).
  void NotifySigninDialogClosed();

  void FlushMojoForTesting();
  int GetNumPendingAccessTokenRequests() const;

  ShowAddAccountDialogCallback account_addition_callback_;
  ShowReauthAccountDialogCallback account_reauth_callback_;
  bool account_signin_in_progress_ = false;
  bool is_reauth_ = false;
  const raw_ptr<account_manager::AccountManager> account_manager_;
  std::unique_ptr<account_manager::AccountManagerUI> account_manager_ui_;
  std::vector<std::unique_ptr<AccessTokenFetcher>>
      pending_access_token_requests_;

  // Don't add new members below this. `receivers_` and `observers_` should be
  // destroyed as soon as `this` is getting destroyed so that we don't deal
  // with message handling on a partially destroyed object.
  mojo::ReceiverSet<mojom::AccountManager> receivers_;
  mojo::RemoteSet<mojom::AccountManagerObserver> observers_;

  base::WeakPtrFactory<AccountManagerMojoService> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_MOJO_SERVICE_H_
