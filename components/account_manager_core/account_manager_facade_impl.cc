// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

namespace account_manager {

namespace {

using RemoteMinVersions = crosapi::mojom::AccountManager::MethodMinVersions;

// UMA histogram names.
const char kMojoDisconnectionsAccountManagerRemote[] =
    "AccountManager.MojoDisconnections.AccountManagerRemote";

// Error logs the Mojo connection stats when `event` occurs.
void LogMojoConnectionStats(const std::string& event,
                            int num_remote_disconnections) {
  LOG(ERROR) << base::StringPrintf("%s. Number of remote disconnections: %d",
                                   event.c_str(), num_remote_disconnections);
}

}  // namespace

AccountManagerFacadeImpl::AccountManagerFacadeImpl(
    mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
    uint32_t remote_version,
    AccountManager* account_manager,
    base::OnceClosure init_finished)
    : remote_version_(remote_version),
      account_manager_remote_(std::move(account_manager_remote)),
      account_manager_(CHECK_DEREF(account_manager)) {
  DCHECK(init_finished);

  account_manager_observation_.Observe(account_manager);

  if (account_manager_remote_) {
    account_manager_remote_.set_disconnect_handler(base::BindOnce(
        &AccountManagerFacadeImpl::OnAccountManagerRemoteDisconnected,
        weak_factory_.GetWeakPtr()));
  }

  std::move(init_finished).Run();
}

AccountManagerFacadeImpl::~AccountManagerFacadeImpl() {
  base::UmaHistogramCounts100(kMojoDisconnectionsAccountManagerRemote,
                              num_remote_disconnections_);
}

void AccountManagerFacadeImpl::AddObserver(
    AccountManagerFacade::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountManagerFacadeImpl::RemoveObserver(
    AccountManagerFacade::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AccountManagerFacadeImpl::GetAccounts(
    base::OnceCallback<void(const std::vector<Account>&)> callback) {
  account_manager_->GetAccounts(base::BindOnce(
      [](base::OnceCallback<void(const std::vector<Account>&)> callback,
         const std::vector<Account>& accounts) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), accounts));
      },
      std::move(callback)));
}

void AccountManagerFacadeImpl::GetPersistentErrorForAccount(
    const AccountKey& account,
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback) {
  account_manager_->HasDummyGaiaToken(
      account,
      base::BindOnce(
          [](base::OnceCallback<void(const GoogleServiceAuthError&)> callback,
             bool has_dummy_token) {
            GoogleServiceAuthError error =
                has_dummy_token
                    ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                              CREDENTIALS_REJECTED_BY_CLIENT)
                    : GoogleServiceAuthError::AuthErrorNone();
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), error));
          },
          std::move(callback)));
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountManagerFacadeImpl::CreateAccessTokenFetcher(
    const AccountKey& account,
    OAuth2AccessTokenConsumer* consumer) {
  return account_manager_->CreateAccessTokenFetcher(account, consumer);
}

void AccountManagerFacadeImpl::ReportAuthError(
    const account_manager::AccountKey& account,
    const GoogleServiceAuthError& error) {
  // Silently ignore transient errors reported by apps to avoid polluting
  // other apps' error caches with transient errors like
  // `GoogleServiceAuthError::CONNECTION_FAILED`.
  if (error.IsTransientError()) {
    return;
  }

  for (auto& observer : observer_list_) {
    observer.OnAuthErrorChanged(account, error);
  }
}

void AccountManagerFacadeImpl::UpsertAccountForTesting(
    const Account& account,
    const std::string& token_value) {
  CHECK_IS_TEST();
  account_manager_->UpsertAccount(account.key, account.raw_email, token_value);
}

void AccountManagerFacadeImpl::RemoveAccountForTesting(
    const AccountKey& account) {
  CHECK_IS_TEST();
  account_manager_->RemoveAccount(account);
}

void AccountManagerFacadeImpl::OnTokenUpserted(const Account& account) {
  observer_list_.Notify(&AccountManagerFacade::Observer::OnAccountUpserted,
                        account);
}

void AccountManagerFacadeImpl::OnAccountRemoved(const Account& account) {
  observer_list_.Notify(&AccountManagerFacade::Observer::OnAccountRemoved,
                        account);
}

void AccountManagerFacadeImpl::RunOnAccountManagerRemoteDisconnection(
    base::OnceClosure closure) {
  if (!account_manager_remote_) {
    std::move(closure).Run();
    return;
  }
  account_manager_remote_disconnection_handlers_.emplace_back(
      std::move(closure));
}

void AccountManagerFacadeImpl::OnAccountManagerRemoteDisconnected() {
  num_remote_disconnections_++;
  LogMojoConnectionStats("Account Manager disconnected",
                         num_remote_disconnections_);
  for (auto& cb : account_manager_remote_disconnection_handlers_) {
    std::move(cb).Run();
  }
  account_manager_remote_disconnection_handlers_.clear();
  account_manager_remote_.reset();
}

void AccountManagerFacadeImpl::FlushMojoForTesting() {
  if (!account_manager_remote_) {
    return;
  }
  account_manager_remote_.FlushForTesting();
}

}  // namespace account_manager
