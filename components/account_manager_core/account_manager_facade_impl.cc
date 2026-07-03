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
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"

namespace account_manager {

namespace {

using RemoteMinVersions = crosapi::mojom::AccountManager::MethodMinVersions;

// UMA histogram names.
const char kMojoDisconnectionsAccountManagerRemote[] =
    "AccountManager.MojoDisconnections.AccountManagerRemote";
const char kMojoDisconnectionsAccountManagerObserverReceiver[] =
    "AccountManager.MojoDisconnections.AccountManagerObserverReceiver";
const char kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote[] =
    "AccountManager.MojoDisconnections.AccessTokenFetcherRemote";

void UnmarshalPersistentError(
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback,
    crosapi::mojom::GoogleServiceAuthErrorPtr mojo_error) {
  std::optional<GoogleServiceAuthError> maybe_error =
      FromMojoGoogleServiceAuthError(mojo_error);
  if (!maybe_error) {
    // Couldn't unmarshal GoogleServiceAuthError, report the account as not
    // having an error. This is safe to do, as GetPersistentErrorForAccount is
    // best-effort (there's no way to know that the token was revoked on the
    // server).
    std::move(callback).Run(GoogleServiceAuthError::AuthErrorNone());
    return;
  }
  std::move(callback).Run(maybe_error.value());
}

// Error logs the Mojo connection stats when `event` occurs.
void LogMojoConnectionStats(const std::string& event,
                            int num_remote_disconnections,
                            int num_receiver_disconnections) {
  LOG(ERROR) << base::StringPrintf(
      "%s. Number of remote disconnections: %d, "
      "number of receiver disconnections: %d",
      event.c_str(), num_remote_disconnections, num_receiver_disconnections);
}

}  // namespace

// Fetches access tokens over the Mojo remote to `AccountManager`.
class AccountManagerFacadeImpl::AccessTokenFetcher
    : public OAuth2AccessTokenFetcher {
 public:
  AccessTokenFetcher(AccountManagerFacadeImpl* account_manager_facade_impl,
                     const account_manager::AccountKey& account_key,
                     OAuth2AccessTokenConsumer* consumer)
      : OAuth2AccessTokenFetcher(consumer),
        account_manager_facade_impl_(account_manager_facade_impl),
        account_key_(account_key),
        oauth_consumer_name_(consumer->GetConsumerName()) {}

  AccessTokenFetcher(const AccessTokenFetcher&) = delete;
  AccessTokenFetcher& operator=(const AccessTokenFetcher&) = delete;

  ~AccessTokenFetcher() override {
    base::UmaHistogramCounts100(
        kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote,
        num_remote_disconnections_);
  }

  // Returns a closure, which marks `this` instance as ready for use. This
  // happens when `AccountManagerFacadeImpl`'s initialization sequence is
  // complete.
  base::OnceClosure UnblockTokenRequest() {
    return base::BindOnce(&AccessTokenFetcher::UnblockTokenRequestInternal,
                          weak_factory_.GetWeakPtr());
  }

  // Returns a closure which handles Mojo connection errors tied to Account
  // Manager remote.
  base::OnceClosure AccountManagerRemoteDisconnectionClosure() {
    return base::BindOnce(
        &AccessTokenFetcher::OnAccountManagerRemoteDisconnection,
        weak_factory_.GetWeakPtr());
  }

  // OAuth2AccessTokenFetcher override:
  // Note: This implementation ignores `client_id` and `client_secret` because
  // AccountManager's Mojo API does not support overriding OAuth client id and
  // secret.
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override {
    DCHECK(!is_request_pending_);
    is_request_pending_ = true;
    scopes_ = scopes;
    if (!are_token_requests_allowed_) {
      return;
    }
    StartInternal();
  }

  // OAuth2AccessTokenFetcher override:
  void CancelRequest() override {
    access_token_fetcher_.reset();
    is_request_pending_ = false;
  }

 private:
  void UnblockTokenRequestInternal() {
    are_token_requests_allowed_ = true;
    if (is_request_pending_) {
      StartInternal();
    }
  }

  void StartInternal() {
    DCHECK(are_token_requests_allowed_);
    bool is_remote_connected =
        account_manager_facade_impl_->CreateAccessTokenFetcher(
            account_manager::ToMojoAccountKey(account_key_),
            oauth_consumer_name_,
            base::BindOnce(&AccessTokenFetcher::FetchAccessToken,
                           weak_factory_.GetWeakPtr()));

    if (!is_remote_connected) {
      OnAccountManagerRemoteDisconnection();
    }
  }

  void FetchAccessToken(
      mojo::PendingRemote<crosapi::mojom::AccessTokenFetcher> pending_remote) {
    access_token_fetcher_.Bind(std::move(pending_remote));
    access_token_fetcher_.set_disconnect_handler(base::BindOnce(
        &AccessTokenFetcher::OnAccessTokenFetcherRemoteDisconnection,
        weak_factory_.GetWeakPtr()));
    access_token_fetcher_->Start(
        scopes_, base::BindOnce(&AccessTokenFetcher::OnAccessTokenFetchComplete,
                                weak_factory_.GetWeakPtr()));
  }

  void OnAccessTokenFetchComplete(crosapi::mojom::AccessTokenResultPtr result) {
    DCHECK(is_request_pending_);
    is_request_pending_ = false;

    if (result->is_error()) {
      std::optional<GoogleServiceAuthError> maybe_error =
          account_manager::FromMojoGoogleServiceAuthError(result->get_error());

      if (!maybe_error.has_value()) {
        LOG(ERROR) << "Unable to parse error result of access token fetch: "
                   << result->get_error()->state;
        FireOnGetTokenFailure(
            GoogleServiceAuthError::FromUnexpectedServiceResponse(
                "Error parsing Mojo error result of access token fetch"));
      } else {
        FireOnGetTokenFailure(maybe_error.value());
      }
      return;
    }

    FireOnGetTokenSuccess(
        OAuth2AccessTokenConsumer::TokenResponse::Builder()
            .WithAccessToken(result->get_access_token_info()->access_token)
            .WithExpirationTime(
                result->get_access_token_info()->expiration_time)
            .WithIdToken(result->get_access_token_info()->id_token)
            .build());
  }

  void OnAccountManagerRemoteDisconnection() {
    FailPendingRequestWithServiceError("Mojo pipe disconnected");
  }

  void OnAccessTokenFetcherRemoteDisconnection() {
    num_remote_disconnections_++;
    LOG(ERROR) << "Access token fetcher remote disconnected";
    FailPendingRequestWithServiceError("Access token Mojo pipe disconnected");
  }

  void FailPendingRequestWithServiceError(const std::string& message) {
    if (!is_request_pending_)
      return;

    CancelRequest();
    FireOnGetTokenFailure(GoogleServiceAuthError::FromServiceError(message));
  }

  const raw_ptr<AccountManagerFacadeImpl> account_manager_facade_impl_;
  const account_manager::AccountKey account_key_;
  const std::string oauth_consumer_name_;

  bool are_token_requests_allowed_ = false;
  bool is_request_pending_ = false;
  // Number of Mojo pipe disconnections seen by `access_token_fetcher_`.
  int num_remote_disconnections_ = 0;
  std::vector<std::string> scopes_;
  mojo::Remote<crosapi::mojom::AccessTokenFetcher> access_token_fetcher_;

  base::WeakPtrFactory<AccessTokenFetcher> weak_factory_{this};
};

AccountManagerFacadeImpl::AccountManagerFacadeImpl(
    mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
    uint32_t remote_version,
    AccountManager* account_manager,
    base::OnceClosure init_finished)
    : remote_version_(remote_version),
      account_manager_remote_(std::move(account_manager_remote)),
      account_manager_(CHECK_DEREF(account_manager)) {
  DCHECK(init_finished);
  initialization_callbacks_.emplace_back(std::move(init_finished));

  account_manager_observation_.Observe(account_manager);

  if (!account_manager_remote_ ||
      remote_version_ < RemoteMinVersions::kAddObserverMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_
                 << ", expected: " << RemoteMinVersions::kAddObserverMinVersion
                 << ". Account consistency will be disabled";
    FinishInitSequenceIfNotAlreadyFinished();
    return;
  }

  account_manager_remote_.set_disconnect_handler(base::BindOnce(
      &AccountManagerFacadeImpl::OnAccountManagerRemoteDisconnected,
      weak_factory_.GetWeakPtr()));
  account_manager_remote_->AddObserver(
      base::BindOnce(&AccountManagerFacadeImpl::OnReceiverReceived,
                     weak_factory_.GetWeakPtr()));
}

AccountManagerFacadeImpl::~AccountManagerFacadeImpl() {
  base::UmaHistogramCounts100(kMojoDisconnectionsAccountManagerRemote,
                              num_remote_disconnections_);
  base::UmaHistogramCounts100(kMojoDisconnectionsAccountManagerObserverReceiver,
                              num_receiver_disconnections_);
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
  if (!account_manager_remote_ ||
      remote_version_ <
          RemoteMinVersions::kGetPersistentErrorForAccountMinVersion) {
    // Remote side doesn't support GetPersistentErrorForAccount.
    std::move(callback).Run(GoogleServiceAuthError::AuthErrorNone());
    return;
  }
  RunAfterInitializationSequence(
      base::BindOnce(&AccountManagerFacadeImpl::GetPersistentErrorInternal,
                     weak_factory_.GetWeakPtr(), account, std::move(callback)));
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountManagerFacadeImpl::CreateAccessTokenFetcher(
    const AccountKey& account,
    OAuth2AccessTokenConsumer* consumer) {
  if (!account_manager_remote_ ||
      remote_version_ <
          RemoteMinVersions::kCreateAccessTokenFetcherMinVersion) {
    VLOG(1) << "Found remote at: " << remote_version_ << ", expected: "
            << RemoteMinVersions::kCreateAccessTokenFetcherMinVersion
            << " for CreateAccessTokenFetcher";
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer,
        GoogleServiceAuthError::FromServiceError("Mojo pipe disconnected"));
  }

  auto access_token_fetcher = std::make_unique<AccessTokenFetcher>(
      /*account_manager_facade_impl=*/this, account, consumer);
  RunAfterInitializationSequence(access_token_fetcher->UnblockTokenRequest());
  RunOnAccountManagerRemoteDisconnection(
      access_token_fetcher->AccountManagerRemoteDisconnectionClosure());
  return std::move(access_token_fetcher);
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
  // Defer execution until Mojo observers are ready.
  RunAfterInitializationSequence(base::BindOnce(
      [](base::WeakPtr<AccountManagerFacadeImpl> self, const Account& account,
         const std::string& token_value) {
        if (self) {
          self->account_manager_->UpsertAccount(account.key, account.raw_email,
                                                token_value);
        }
      },
      weak_factory_.GetWeakPtr(), account, token_value));
}

void AccountManagerFacadeImpl::RemoveAccountForTesting(
    const AccountKey& account) {
  CHECK_IS_TEST();
  // Defer execution until Mojo observers are ready.
  RunAfterInitializationSequence(base::BindOnce(
      [](base::WeakPtr<AccountManagerFacadeImpl> self,
         const AccountKey& account) {
        if (self) {
          self->account_manager_->RemoveAccount(account);
        }
      },
      weak_factory_.GetWeakPtr(), account));
}

void AccountManagerFacadeImpl::OnReceiverReceived(
    mojo::PendingReceiver<AccountManagerObserver> receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>(
          this, std::move(receiver));
  // At this point (`receiver_` exists), we are subscribed to Account Manager.

  receiver_->set_disconnect_handler(base::BindOnce(
      &AccountManagerFacadeImpl::OnAccountManagerObserverReceiverDisconnected,
      weak_factory_.GetWeakPtr()));

  FinishInitSequenceIfNotAlreadyFinished();
}

void AccountManagerFacadeImpl::OnTokenUpserted(const Account& account) {
  observer_list_.Notify(&AccountManagerFacade::Observer::OnAccountUpserted,
                        account);
}

void AccountManagerFacadeImpl::OnAccountRemoved(const Account& account) {
  observer_list_.Notify(&AccountManagerFacade::Observer::OnAccountRemoved,
                        account);
}

void AccountManagerFacadeImpl::OnSigninDialogClosed() {
  observer_list_.Notify(&AccountManagerFacade::Observer::OnSigninDialogClosed);
}

void AccountManagerFacadeImpl::GetPersistentErrorInternal(
    const AccountKey& account,
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback) {
  account_manager_remote_->GetPersistentErrorForAccount(
      ToMojoAccountKey(account),
      base::BindOnce(&UnmarshalPersistentError, std::move(callback)));
}

bool AccountManagerFacadeImpl::CreateAccessTokenFetcher(
    crosapi::mojom::AccountKeyPtr account_key,
    const std::string& oauth_consumer_name,
    crosapi::mojom::AccountManager::CreateAccessTokenFetcherCallback callback) {
  if (!account_manager_remote_) {
    return false;
  }

  account_manager_remote_->CreateAccessTokenFetcher(
      std::move(account_key), oauth_consumer_name, std::move(callback));
  return true;
}

void AccountManagerFacadeImpl::FinishInitSequenceIfNotAlreadyFinished() {
  if (is_initialized_) {
    return;
  }

  is_initialized_ = true;
  for (auto& cb : initialization_callbacks_) {
    std::move(cb).Run();
  }
  initialization_callbacks_.clear();
}

void AccountManagerFacadeImpl::RunAfterInitializationSequence(
    base::OnceClosure closure) {
  if (!is_initialized_) {
    initialization_callbacks_.emplace_back(std::move(closure));
  } else {
    std::move(closure).Run();
  }
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
                         num_remote_disconnections_,
                         num_receiver_disconnections_);
  for (auto& cb : account_manager_remote_disconnection_handlers_) {
    std::move(cb).Run();
  }
  account_manager_remote_disconnection_handlers_.clear();
  account_manager_remote_.reset();
}

void AccountManagerFacadeImpl::OnAccountManagerObserverReceiverDisconnected() {
  num_receiver_disconnections_++;
  LogMojoConnectionStats("Account Manager Observer disconnected",
                         num_remote_disconnections_,
                         num_receiver_disconnections_);
}

bool AccountManagerFacadeImpl::IsInitialized() {
  return is_initialized_;
}

void AccountManagerFacadeImpl::FlushMojoForTesting() {
  if (!account_manager_remote_) {
    return;
  }
  account_manager_remote_.FlushForTesting();
}

}  // namespace account_manager
