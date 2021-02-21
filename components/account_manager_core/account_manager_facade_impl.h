// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace account_manager {

// ChromeOS-specific implementation of |AccountManagerFacade| that talks to
// |ash::AccountManager| over Mojo. Used by both Lacros and Ash.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacadeImpl
    : public AccountManagerFacade,
      public crosapi::mojom::AccountManagerObserver {
 public:
  // Constructs `AccountManagerFacadeImpl`.
  // `account_manager_remote` is a Mojo `Remote` to Account Manager in Ash -
  // either in-process or out-of-process.
  // `remote_version` is the Mojo API version of the remote.
  // `init_finished` is called after Account Manager has been fully initialized.
  AccountManagerFacadeImpl(
      mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
      uint32_t remote_version,
      base::OnceClosure init_finished = base::DoNothing());
  AccountManagerFacadeImpl(const AccountManagerFacadeImpl&) = delete;
  AccountManagerFacadeImpl& operator=(const AccountManagerFacadeImpl&) = delete;
  ~AccountManagerFacadeImpl() override;

  // AccountManagerFacade overrides:
  bool IsInitialized() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetAccounts(
      base::OnceCallback<void(const std::vector<Account>&)> callback) override;
  void ShowAddAccountDialog(const AccountAdditionSource& source) override;
  void ShowAddAccountDialog(
      const AccountAdditionSource& source,
      base::OnceCallback<void(const account_manager::AccountAdditionResult&
                                  result)> callback) override;
  void ShowReauthAccountDialog(const AccountAdditionSource& source,
                               const std::string& email) override;
  void ShowManageAccountsSettings() override;

  // crosapi::mojom::AccountManagerObserver overrides:
  void OnTokenUpserted(crosapi::mojom::AccountPtr account) override;
  void OnAccountRemoved(crosapi::mojom::AccountPtr account) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowAddAccountDialogCallsMojo);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowAddAccountDialogUMA);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowReauthAccountDialogCallsMojo);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowReauthAccountDialogUMA);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerFacadeImplTest,
                           ShowManageAccountsSettingsCallsMojo);
  static std::string GetAccountAdditionResultStatusHistogramNameForTesting();

  void OnReceiverReceived(
      mojo::PendingReceiver<AccountManagerObserver> receiver);
  void OnInitialized(bool is_initialized);
  // Callback for `crosapi::mojom::AccountManager::ShowAddAccountDialog`.
  void OnShowAddAccountDialogFinished(
      base::OnceCallback<
          void(const account_manager::AccountAdditionResult& result)> callback,
      crosapi::mojom::AccountAdditionResultPtr mojo_result);
  void FinishAddAccount(
      base::OnceCallback<
          void(const account_manager::AccountAdditionResult& result)> callback,
      const account_manager::AccountAdditionResult& result);

  void FlushMojoForTesting();

  void GetAccountsInternal(
      base::OnceCallback<void(const std::vector<Account>&)> callback);

  // Mojo API version on the remote (Ash) side.
  const uint32_t remote_version_;

  // The initialization sequence for |AccountManagerFacadeImpl| consists of:
  // 1. Adding an observer to the remote.
  // 2. Querying the remote initialization status.
  //
  // Remote-querying methods like |GetAccounts| won't actually produce a remote
  // call until the initialization sequence is finished (instead, they will be
  // queued in |initialization_callbacks_|).
  //
  // |FinishInitSequenceIfNotAlreadyFinished| invokes callbacks from
  // |initialization_callbacks_| and marks the initialization as finished.
  void FinishInitSequenceIfNotAlreadyFinished();
  // |closure| will be invoked after the initialization sequence is finished.
  // See |FinishInitSequenceIfNotAlreadyFinished| for details.
  void RunAfterInitializationSequence(base::OnceClosure closure);

  bool is_remote_initialized_ = false;
  bool is_initialization_sequence_finished_ = false;
  std::vector<base::OnceClosure> initialization_callbacks_;

  mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote_;
  std::unique_ptr<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>
      receiver_;

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<AccountManagerFacadeImpl> weak_factory_{this};
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
