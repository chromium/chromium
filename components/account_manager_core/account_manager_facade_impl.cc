// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_util.h"

namespace account_manager {

namespace {

// UMA histogram name.
const char kAccountAdditionResultStatus[] =
    "AccountManager.AccountAdditionResultStatus";

// Interface versions in //chromeos/crosapi/mojom/account_manager.mojom:
// MinVersion of crosapi::mojom::AccountManager::GetAccounts
constexpr uint32_t kMinVersionWithGetAccounts = 2;
// MinVersion of crosapi::mojom::AccountManager::ShowAddAccountDialog and
// crosapi::mojom::AccountManager::ShowReauthAccountDialog.
constexpr uint32_t kMinVersionWithShowAddAccountDialog = 3;
// MinVersion of crosapi::mojom::AccountManager::ShowManageAccountsSettings.
constexpr uint32_t kMinVersionWithManageAccountsSettings = 4;

void UnmarshalAccounts(
    base::OnceCallback<void(const std::vector<Account>&)> callback,
    std::vector<crosapi::mojom::AccountPtr> mojo_accounts) {
  std::vector<Account> accounts;
  for (const auto& mojo_account : mojo_accounts) {
    base::Optional<Account> maybe_account = FromMojoAccount(mojo_account);
    if (!maybe_account) {
      // Skip accounts we couldn't unmarshal. No logging, as it would produce
      // a lot of noise.
      continue;
    }
    accounts.emplace_back(std::move(maybe_account.value()));
  }
  std::move(callback).Run(std::move(accounts));
}

}  // namespace

AccountManagerFacadeImpl::AccountManagerFacadeImpl(
    mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
    uint32_t remote_version,
    base::OnceClosure init_finished)
    : remote_version_(remote_version),
      account_manager_remote_(std::move(account_manager_remote)) {
  DCHECK(init_finished);
  initialization_callbacks_.emplace_back(std::move(init_finished));

  if (!account_manager_remote_ ||
      remote_version_ < kMinVersionWithGetAccounts) {
    LOG(WARNING) << "Found remote at: " << remote_version_
                 << ", expected: " << kMinVersionWithGetAccounts
                 << ". Account consistency will be disabled";
    FinishInitSequenceIfNotAlreadyFinished();
    return;
  }
  account_manager_remote_->AddObserver(
      base::BindOnce(&AccountManagerFacadeImpl::OnReceiverReceived,
                     weak_factory_.GetWeakPtr()));
}

AccountManagerFacadeImpl::~AccountManagerFacadeImpl() = default;

bool AccountManagerFacadeImpl::IsInitialized() {
  return is_remote_initialized_;
}

void AccountManagerFacadeImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountManagerFacadeImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AccountManagerFacadeImpl::GetAccounts(
    base::OnceCallback<void(const std::vector<Account>&)> callback) {
  if (!account_manager_remote_ ||
      remote_version_ < kMinVersionWithGetAccounts) {
    // Remote side doesn't support GetAccounts, return an empty list.
    std::move(callback).Run({});
    return;
  }
  RunAfterInitializationSequence(
      base::BindOnce(&AccountManagerFacadeImpl::GetAccountsInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AccountManagerFacadeImpl::ShowAddAccountDialog(
    const AccountAdditionSource& source) {
  ShowAddAccountDialog(
      source,
      base::DoNothing::Once<const account_manager::AccountAdditionResult&>());
}

void AccountManagerFacadeImpl::ShowAddAccountDialog(
    const AccountAdditionSource& source,
    base::OnceCallback<
        void(const account_manager::AccountAdditionResult& result)> callback) {
  if (remote_version_ < kMinVersionWithShowAddAccountDialog) {
    LOG(WARNING) << "Found remote at: " << remote_version_
                 << ", expected: " << kMinVersionWithShowAddAccountDialog
                 << " for ShowAddAccountDialog.";
    FinishAddAccount(std::move(callback),
                     account_manager::AccountAdditionResult(
                         account_manager::AccountAdditionResult::Status::
                             kUnexpectedResponse));
    return;
  }

  base::UmaHistogramEnumeration(kAccountAdditionSource, source);

  account_manager_remote_->ShowAddAccountDialog(
      base::BindOnce(&AccountManagerFacadeImpl::OnShowAddAccountDialogFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AccountManagerFacadeImpl::ShowReauthAccountDialog(
    const AccountAdditionSource& source,
    const std::string& email) {
  if (remote_version_ < kMinVersionWithShowAddAccountDialog) {
    LOG(WARNING) << "Found remote at: " << remote_version_
                 << ", expected: " << kMinVersionWithShowAddAccountDialog
                 << " for ShowReauthAccountDialog.";
    return;
  }

  base::UmaHistogramEnumeration(kAccountAdditionSource, source);

  account_manager_remote_->ShowReauthAccountDialog(email, base::DoNothing());
}

void AccountManagerFacadeImpl::ShowManageAccountsSettings() {
  if (remote_version_ < kMinVersionWithManageAccountsSettings) {
    LOG(WARNING) << "Found remote at: " << remote_version_
                 << ", expected: " << kMinVersionWithManageAccountsSettings
                 << " for ShowManageAccountsSettings.";
    return;
  }

  account_manager_remote_->ShowManageAccountsSettings();
}

// static
std::string AccountManagerFacadeImpl::
    GetAccountAdditionResultStatusHistogramNameForTesting() {
  return kAccountAdditionResultStatus;
}

void AccountManagerFacadeImpl::OnReceiverReceived(
    mojo::PendingReceiver<AccountManagerObserver> receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>(
          this, std::move(receiver));
  // At this point (|receiver_| exists), we are subscribed to Account Manager.

  account_manager_remote_->IsInitialized(base::BindOnce(
      &AccountManagerFacadeImpl::OnInitialized, weak_factory_.GetWeakPtr()));
}

void AccountManagerFacadeImpl::OnInitialized(bool is_initialized) {
  if (is_initialized)
    is_remote_initialized_ = true;
  // else: We will receive a notification in |OnTokenUpserted|.
  FinishInitSequenceIfNotAlreadyFinished();
}

void AccountManagerFacadeImpl::OnShowAddAccountDialogFinished(
    base::OnceCallback<
        void(const account_manager::AccountAdditionResult& result)> callback,
    crosapi::mojom::AccountAdditionResultPtr mojo_result) {
  base::Optional<account_manager::AccountAdditionResult> result =
      account_manager::FromMojoAccountAdditionResult(mojo_result);
  if (!result.has_value()) {
    FinishAddAccount(std::move(callback),
                     account_manager::AccountAdditionResult(
                         account_manager::AccountAdditionResult::Status::
                             kUnexpectedResponse));
    return;
  }
  FinishAddAccount(std::move(callback), result.value());
}

void AccountManagerFacadeImpl::FinishAddAccount(
    base::OnceCallback<
        void(const account_manager::AccountAdditionResult& result)> callback,
    const account_manager::AccountAdditionResult& result) {
  base::UmaHistogramEnumeration(kAccountAdditionResultStatus, result.status);
  std::move(callback).Run(result);
}

void AccountManagerFacadeImpl::OnTokenUpserted(
    crosapi::mojom::AccountPtr account) {
  is_remote_initialized_ = true;
  // |OnTokenUpserted| may be invoked before |OnInitialized|. Invoking
  // initialization sequence callbacks before observers makes sure observers
  // aren't confused by the call order.
  FinishInitSequenceIfNotAlreadyFinished();

  base::Optional<Account> maybe_account = FromMojoAccount(account);
  if (!maybe_account) {
    LOG(WARNING) << "Can't unmarshal account of type: "
                 << account->key->account_type;
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnAccountUpserted(maybe_account->key);
  }
}

void AccountManagerFacadeImpl::OnAccountRemoved(
    crosapi::mojom::AccountPtr account) {
  base::Optional<Account> maybe_account = FromMojoAccount(account);
  if (!maybe_account) {
    LOG(WARNING) << "Can't unmarshal account of type: "
                 << account->key->account_type;
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnAccountRemoved(maybe_account->key);
  }
}

void AccountManagerFacadeImpl::GetAccountsInternal(
    base::OnceCallback<void(const std::vector<Account>&)> callback) {
  account_manager_remote_->GetAccounts(
      base::BindOnce(&UnmarshalAccounts, std::move(callback)));
}

void AccountManagerFacadeImpl::FinishInitSequenceIfNotAlreadyFinished() {
  if (is_initialization_sequence_finished_)
    return;
  is_initialization_sequence_finished_ = true;
  for (auto& cb : initialization_callbacks_) {
    std::move(cb).Run();
  }
  initialization_callbacks_.clear();
}

void AccountManagerFacadeImpl::RunAfterInitializationSequence(
    base::OnceClosure closure) {
  if (!is_initialization_sequence_finished_) {
    initialization_callbacks_.emplace_back(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void AccountManagerFacadeImpl::FlushMojoForTesting() {
  account_manager_remote_.FlushForTesting();
}

}  // namespace account_manager
