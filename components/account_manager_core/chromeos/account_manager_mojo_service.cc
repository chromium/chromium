// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/chromeos/access_token_fetcher.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_ui.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

namespace {

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

void RecordMojoAccountUpsertionResultStatusAndRunCallback(
    base::OnceCallback<void(mojom::AccountUpsertionResultPtr)> callback,
    mojom::AccountUpsertionResultPtr mojo_result) {
  const std::optional<account_manager::AccountUpsertionResult> result =
      account_manager::FromMojoAccountUpsertionResult(mojo_result);
  account_manager::RecordAccountUpsertionResultStatus(
      result.has_value() ? result->status()
                         : account_manager::AccountUpsertionResult::Status::
                               kUnexpectedResponse);
  std::move(callback).Run(std::move(mojo_result));
}

}  // namespace

AccountManagerMojoService::AccountManagerMojoService(
    account_manager::AccountManager* account_manager)
    : account_manager_(account_manager) {
  CHECK(account_manager_);
}

AccountManagerMojoService::~AccountManagerMojoService() = default;

void AccountManagerMojoService::BindReceiver(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AccountManagerMojoService::SetAccountManagerUI(
    std::unique_ptr<account_manager::AccountManagerUI> account_manager_ui) {
  account_manager_ui_ = std::move(account_manager_ui);
}

base::OnceCallback<void(const account_manager::AccountUpsertionResult&)>
AccountManagerMojoService::CreateInlineLoginAccountUpsertionFinishedCallback() {
  return base::BindOnce(&AccountManagerMojoService::OnAccountUpsertionFinished,
                        weak_ptr_factory_.GetWeakPtr());
}

void AccountManagerMojoService::AddObserver(AddObserverCallback callback) {
  mojo::Remote<mojom::AccountManagerObserver> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  observers_.Add(std::move(remote));
  std::move(callback).Run(std::move(receiver));
}

void AccountManagerMojoService::GetPersistentErrorForAccount(
    mojom::AccountKeyPtr mojo_account_key,
    mojom::AccountManager::GetPersistentErrorForAccountCallback callback) {
  std::optional<account_manager::AccountKey> maybe_account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  DCHECK(maybe_account_key)
      << "Can't unmarshal account of type: " << mojo_account_key->account_type;
  account_manager_->HasDummyGaiaToken(
      maybe_account_key.value(),
      base::BindOnce(&ReportErrorStatusFromHasDummyGaiaToken,
                     std::move(callback)));
}

void AccountManagerMojoService::ShowAddAccountDialog(
    account_manager::AccountAdditionSource source,
    crosapi::mojom::AccountAdditionOptionsPtr options,
    ShowAddAccountDialogCallback callback) {
  account_manager::RecordAccountAdditionSource(source);
  ShowAddAccountDialog(
      std::move(options),
      base::BindOnce(&RecordMojoAccountUpsertionResultStatusAndRunCallback,
                     std::move(callback)));
}

void AccountManagerMojoService::ShowAddAccountDialog(
    crosapi::mojom::AccountAdditionOptionsPtr options,
    ShowAddAccountDialogCallback callback) {
  CHECK(account_manager_ui_);
  if (account_manager_ui_->IsDialogShown()) {
    std::move(callback).Run(ToMojoAccountUpsertionResult(
        account_manager::AccountUpsertionResult::FromStatus(
            account_manager::AccountUpsertionResult::Status::
                kAlreadyInProgress)));
    return;
  }

  DCHECK(!account_signin_in_progress_);
  account_signin_in_progress_ = true;
  is_reauth_ = false;
  account_addition_callback_ = std::move(callback);
  auto maybe_options = account_manager::FromMojoAccountAdditionOptions(options);
  account_manager_ui_->ShowAddAccountDialog(
      maybe_options.value_or(account_manager::AccountAdditionOptions{}),
      base::BindOnce(&AccountManagerMojoService::OnSigninDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccountManagerMojoService::ShowReauthAccountDialog(
    account_manager::AccountAdditionSource source,
    const std::string& email,
    ShowReauthAccountDialogCallback callback) {
  account_manager::RecordAccountAdditionSource(source);
  ShowReauthAccountDialog(
      email,
      base::BindOnce(&RecordMojoAccountUpsertionResultStatusAndRunCallback,
                     std::move(callback)));
}

void AccountManagerMojoService::ShowReauthAccountDialog(
    const std::string& email,
    ShowReauthAccountDialogCallback callback) {
  CHECK(account_manager_ui_);
  if (account_manager_ui_->IsDialogShown()) {
    std::move(callback).Run(ToMojoAccountUpsertionResult(
        account_manager::AccountUpsertionResult::FromStatus(
            account_manager::AccountUpsertionResult::Status::
                kAlreadyInProgress)));
    return;
  }

  DCHECK(!account_signin_in_progress_);
  account_signin_in_progress_ = true;
  is_reauth_ = true;
  account_reauth_callback_ = std::move(callback);
  account_manager_ui_->ShowReauthAccountDialog(
      email, base::BindOnce(&AccountManagerMojoService::OnSigninDialogClosed,
                            weak_ptr_factory_.GetWeakPtr()));
}

void AccountManagerMojoService::CreateAccessTokenFetcher(
    mojom::AccountKeyPtr mojo_account_key,
    const std::string& oauth_consumer_name,
    CreateAccessTokenFetcherCallback callback) {
  // TODO(crbug.com/40747515): Add metrics.
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

void AccountManagerMojoService::OnAccountUpsertionFinished(
    const account_manager::AccountUpsertionResult& result) {
  if (!account_signin_in_progress_) {
    return;
  }

  FinishUpsertAccount(result);
}

void AccountManagerMojoService::OnSigninDialogClosed() {
  if (!account_signin_in_progress_) {
    return;
  }

  // Account addition is still in progress. It means that user didn't complete
  // the account addition flow and closed the dialog.
  FinishUpsertAccount(account_manager::AccountUpsertionResult::FromStatus(
      account_manager::AccountUpsertionResult::Status::kCancelledByUser));
}

void AccountManagerMojoService::FinishUpsertAccount(
    const account_manager::AccountUpsertionResult& result) {
  if (!account_signin_in_progress_) {
    return;
  }

  if (is_reauth_) {
    CHECK(account_reauth_callback_);
    std::move(account_reauth_callback_)
        .Run(ToMojoAccountUpsertionResult(result));
  } else {
    CHECK(account_addition_callback_);
    std::move(account_addition_callback_)
        .Run(ToMojoAccountUpsertionResult(result));
  }

  account_signin_in_progress_ = false;
  is_reauth_ = false;
  NotifySigninDialogClosed();
}

void AccountManagerMojoService::DeletePendingAccessTokenFetchRequest(
    AccessTokenFetcher* request) {
  std::erase_if(
      pending_access_token_requests_,
      [&request](const std::unique_ptr<AccessTokenFetcher>& pending_request)
          -> bool { return pending_request.get() == request; });
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
