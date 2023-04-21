// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/chromeos/access_token_fetcher.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_ui.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {

namespace {

void MarshalAccounts(
    mojom::AccountManager::GetAccountsCallback callback,
    const std::vector<account_manager::Account>& accounts_to_marshal) {
  std::vector<mojom::AccountPtr> mojo_accounts;
  for (const account_manager::Account& account : accounts_to_marshal) {
    mojo_accounts.emplace_back(account_manager::ToMojoAccount(account));
  }
  std::move(callback).Run(std::move(mojo_accounts));
}

void ReportErrorStatusFromHasDummyGaiaToken(
    base::OnceCallback<void(mojom::GoogleServiceAuthErrorPtr)> callback,
    bool has_dummy_token) {
  GoogleServiceAuthError error(GoogleServiceAuthError::AuthErrorNone());
  if (has_dummy_token) {
    error = GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
            CREDENTIALS_REJECTED_BY_CLIENT);
  }
  std::move(callback).Run(account_manager::ToMojoGoogleServiceAuthError(error));
}

}  // namespace

AccountManagerMojoService::AccountManagerMojoService(
    account_manager::AccountManager* account_manager)
    : account_manager_(account_manager) {
  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
}

AccountManagerMojoService::~AccountManagerMojoService() {
  account_manager_->RemoveObserver(this);
}

void AccountManagerMojoService::BindReceiver(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AccountManagerMojoService::SetAccountManagerUI(
    std::unique_ptr<account_manager::AccountManagerUI> account_manager_ui) {
  account_manager_ui_ = std::move(account_manager_ui);
}

void AccountManagerMojoService::OnAccountAdditionFinishedForTesting(
    const account_manager::AccountAdditionResult& result) {
  OnAccountAdditionFinished(result);
}

void AccountManagerMojoService::IsInitialized(IsInitializedCallback callback) {
  std::move(callback).Run(account_manager_->IsInitialized());
}

void AccountManagerMojoService::AddObserver(AddObserverCallback callback) {
  mojo::Remote<mojom::AccountManagerObserver> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  observers_.Add(std::move(remote));
  std::move(callback).Run(std::move(receiver));
}

void AccountManagerMojoService::GetAccounts(
    mojom::AccountManager::GetAccountsCallback callback) {
  account_manager_->GetAccounts(
      base::BindOnce(&MarshalAccounts, std::move(callback)));
}

void AccountManagerMojoService::GetPersistentErrorForAccount(
    mojom::AccountKeyPtr mojo_account_key,
    mojom::AccountManager::GetPersistentErrorForAccountCallback callback) {
  absl::optional<account_manager::AccountKey> maybe_account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  DCHECK(maybe_account_key)
      << "Can't unmarshal account of type: " << mojo_account_key->account_type;
  account_manager_->HasDummyGaiaToken(
      maybe_account_key.value(),
      base::BindOnce(&ReportErrorStatusFromHasDummyGaiaToken,
                     std::move(callback)));
}

void AccountManagerMojoService::ShowAddAccountDialog(
    crosapi::mojom::AccountAdditionOptionsPtr options,
    ShowAddAccountDialogCallback callback) {
  DCHECK(account_manager_ui_);
  if (account_manager_ui_->IsDialogShown()) {
    std::move(callback).Run(ToMojoAccountAdditionResult(
        account_manager::AccountAdditionResult::FromStatus(
            account_manager::AccountAdditionResult::Status::
                kAlreadyInProgress)));
    return;
  }

  DCHECK(!account_addition_in_progress_);
  account_addition_in_progress_ = true;
  account_addition_callback_ = std::move(callback);
  auto maybe_options = account_manager::FromMojoAccountAdditionOptions(options);
  account_manager_ui_->ShowAddAccountDialog(
      maybe_options.value_or(account_manager::AccountAdditionOptions{}),
      base::BindOnce(&AccountManagerMojoService::OnAddAccountDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccountManagerMojoService::ShowReauthAccountDialog(
    const std::string& email,
    base::OnceClosure closure) {
  DCHECK(account_manager_ui_);
  if (account_manager_ui_->IsDialogShown())
    return;

  // `closure` is used by the entity which launched the account
  // re-authentication flow in the first place to know about the completion of
  // the flow. The notification that we are going to chain here will inform all
  // observers of `AccountManagerFacade` (see
  // `AccountManagerFacade::Observer::OnSigninDialogClosed()`), that the signin
  // dialog was closed.
  // As of this writing, this notification is used by `AccountReconcilor` to
  // force mint cookies.
  account_manager_ui_->ShowReauthAccountDialog(
      email, std::move(closure).Then(base::BindOnce(
                 &AccountManagerMojoService::NotifySigninDialogClosed,
                 weak_ptr_factory_.GetWeakPtr())));
}

void AccountManagerMojoService::ShowManageAccountsSettings() {
  account_manager_ui_->ShowManageAccountsSettings();
}

void AccountManagerMojoService::CreateAccessTokenFetcher(
    mojom::AccountKeyPtr mojo_account_key,
    const std::string& oauth_consumer_name,
    CreateAccessTokenFetcherCallback callback) {
  // TODO(https://crbug.com/1175741): Add metrics.
  VLOG(1) << "Received a request for access token from: "
          << oauth_consumer_name;

  mojo::PendingRemote<mojom::AccessTokenFetcher> pending_remote;
  auto access_token_fetcher = std::make_unique<AccessTokenFetcher>(
      account_manager_, std::move(mojo_account_key), oauth_consumer_name,
      /*done_closure=*/
      base::BindOnce(
          &AccountManagerMojoService::DeletePendingAccessTokenFetchRequest,
          weak_ptr_factory_.GetWeakPtr()),
      /*receiver=*/pending_remote.InitWithNewPipeAndPassReceiver());
  pending_access_token_requests_.emplace_back(std::move(access_token_fetcher));
  std::move(callback).Run(std::move(pending_remote));
}

void AccountManagerMojoService::ReportAuthError(
    mojom::AccountKeyPtr mojo_account_key,
    mojom::GoogleServiceAuthErrorPtr mojo_error) {
  absl::optional<account_manager::AccountKey> maybe_account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  base::UmaHistogramBoolean("AccountManager.ReportAuthError.IsAccountKeyEmpty",
                            !maybe_account_key.has_value());
  if (!maybe_account_key) {
    LOG(ERROR) << "Can't unmarshal account with id: " << mojo_account_key->id
               << " and type: " << mojo_account_key->account_type;
    return;
  }

  absl::optional<GoogleServiceAuthError> maybe_error =
      account_manager::FromMojoGoogleServiceAuthError(mojo_error);
  if (!maybe_error) {
    // Newer version of Lacros may have reported an error that older version of
    // Ash doesn't understand yet. Ignore such errors.
    LOG(ERROR) << "Can't unmarshal error with state: " << mojo_error->state;
    return;
  }

  const GoogleServiceAuthError& error = maybe_error.value();
  if (error.IsTransientError()) {
    // Silently ignore transient errors reported by apps to avoid polluting
    // other apps' error caches with transient errors like
    // `GoogleServiceAuthError::CONNECTION_FAILED`.
    return;
  }

  account_manager_->GetAccounts(base::BindOnce(
      &AccountManagerMojoService::MaybeNotifyAuthErrorObservers,
      weak_ptr_factory_.GetWeakPtr(), maybe_account_key.value(), error));
}

void AccountManagerMojoService::OnTokenUpserted(
    const account_manager::Account& account) {
  for (auto& observer : observers_)
    observer->OnTokenUpserted(ToMojoAccount(account));
}

void AccountManagerMojoService::OnAccountRemoved(
    const account_manager::Account& account) {
  for (auto& observer : observers_)
    observer->OnAccountRemoved(ToMojoAccount(account));
}

void AccountManagerMojoService::OnAccountAdditionFinished(
    const account_manager::AccountAdditionResult& result) {
  if (!account_addition_in_progress_)
    return;

  FinishAddAccount(result);
}

void AccountManagerMojoService::OnAddAccountDialogClosed() {
  if (!account_addition_in_progress_)
    return;

  // Account addition is still in progress. It means that user didn't complete
  // the account addition flow and closed the dialog.
  FinishAddAccount(account_manager::AccountAdditionResult::FromStatus(
      account_manager::AccountAdditionResult::Status::kCancelledByUser));
}

void AccountManagerMojoService::FinishAddAccount(
    const account_manager::AccountAdditionResult& result) {
  account_addition_in_progress_ = false;

  DCHECK(!account_addition_callback_.is_null());
  std::move(account_addition_callback_)
      .Run(ToMojoAccountAdditionResult(result));
  NotifySigninDialogClosed();
}

void AccountManagerMojoService::DeletePendingAccessTokenFetchRequest(
    AccessTokenFetcher* request) {
  pending_access_token_requests_.erase(
      std::remove_if(
          pending_access_token_requests_.begin(),
          pending_access_token_requests_.end(),
          [&request](const std::unique_ptr<AccessTokenFetcher>& pending_request)
              -> bool { return pending_request.get() == request; }),
      pending_access_token_requests_.end());
}

void AccountManagerMojoService::MaybeNotifyAuthErrorObservers(
    const account_manager::AccountKey& account_key,
    const GoogleServiceAuthError& error,
    const std::vector<account_manager::Account>& known_accounts) {
  if (!base::Contains(known_accounts, account_key,
                      [](const account_manager::Account& account) {
                        return account.key;
                      })) {
    // Ignore if the account is not known.
    return;
  }

  for (auto& observer : observers_) {
    observer->OnAuthErrorChanged(
        account_manager::ToMojoAccountKey(account_key),
        account_manager::ToMojoGoogleServiceAuthError(error));
  }
}

void AccountManagerMojoService::NotifySigninDialogClosed() {
  for (auto& observer : observers_) {
    observer->OnSigninDialogClosed();
  }
}

void AccountManagerMojoService::FlushMojoForTesting() {
  observers_.FlushForTesting();
}

int AccountManagerMojoService::GetNumPendingAccessTokenRequests() const {
  return pending_access_token_requests_.size();
}

}  // namespace crosapi
