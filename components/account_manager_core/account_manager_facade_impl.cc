// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace account_manager {

namespace {

using RemoteMinVersions = crosapi::mojom::AccountManager::MethodMinVersions;

// UMA histogram names.
const char kAccountAdditionResultStatus[] =
    "AccountManager.AccountAdditionResultStatus";
const char kGetAccountsMojoStatus[] =
    "AccountManager.FacadeGetAccountsMojoStatus";

void UnmarshalAccounts(
    base::OnceCallback<void(const std::vector<Account>&)> callback,
    std::vector<crosapi::mojom::AccountPtr> mojo_accounts) {
  std::vector<Account> accounts;
  for (const auto& mojo_account : mojo_accounts) {
    absl::optional<Account> maybe_account = FromMojoAccount(mojo_account);
    if (!maybe_account) {
      // Skip accounts we couldn't unmarshal. No logging, as it would produce
      // a lot of noise.
      continue;
    }
    accounts.emplace_back(std::move(maybe_account.value()));
  }
  std::move(callback).Run(std::move(accounts));
}

void UnmarshalPersistentError(
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback,
    crosapi::mojom::GoogleServiceAuthErrorPtr mojo_error) {
  absl::optional<GoogleServiceAuthError> maybe_error =
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

// Returns whether an account should be available in ARC after it's added
// in-session.
bool GetIsAvailableInArcBySource(
    AccountManagerFacade::AccountAdditionSource source) {
  switch (source) {
    // Accounts added from Ash should be available in ARC.
    case AccountManagerFacade::AccountAdditionSource::kSettingsAddAccountButton:
    case AccountManagerFacade::AccountAdditionSource::
        kAccountManagerMigrationWelcomeScreen:
    case AccountManagerFacade::AccountAdditionSource::kArc:
    case AccountManagerFacade::AccountAdditionSource::kOnboarding:
      return true;
    // Accounts added from the browser should not be available in ARC.
    case AccountManagerFacade::AccountAdditionSource::kChromeProfileCreation:
    case AccountManagerFacade::AccountAdditionSource::kOgbAddAccount:
      return false;
    // These are reauthentication cases. ARC visibility shouldn't change for
    // reauthentication.
    case AccountManagerFacade::AccountAdditionSource::kContentAreaReauth:
    case AccountManagerFacade::AccountAdditionSource::
        kSettingsReauthAccountButton:
    case AccountManagerFacade::AccountAdditionSource::
        kAvatarBubbleReauthAccountButton:
    case AccountManagerFacade::AccountAdditionSource::kChromeExtensionReauth:
      NOTREACHED();
      return false;
    // Unused enums that cannot be deleted.
    case AccountManagerFacade::AccountAdditionSource::kPrintPreviewDialogUnused:
      NOTREACHED();
      return false;
  }
}

}  // namespace

// Fetches access tokens over the Mojo remote to `AccountManager`.
class AccountManagerFacadeImpl::AccessTokenFetcher
    : public OAuth2AccessTokenFetcher {
 public:
  AccessTokenFetcher(AccountManagerFacadeImpl* account_manager_facade_impl,
                     const account_manager::AccountKey& account_key,
                     const std::string& oauth_consumer_name,
                     OAuth2AccessTokenConsumer* consumer)
      : OAuth2AccessTokenFetcher(consumer),
        account_manager_facade_impl_(account_manager_facade_impl),
        account_key_(account_key),
        oauth_consumer_name_(oauth_consumer_name) {}

  AccessTokenFetcher(const AccessTokenFetcher&) = delete;
  AccessTokenFetcher& operator=(const AccessTokenFetcher&) = delete;

  ~AccessTokenFetcher() override = default;

  // Returns a closure, which marks `this` instance as ready for use. This
  // happens when `AccountManagerFacadeImpl`'s initialization sequence is
  // complete.
  base::OnceClosure UnblockTokenRequest() {
    return base::BindOnce(&AccessTokenFetcher::UnblockTokenRequestInternal,
                          weak_factory_.GetWeakPtr());
  }

  // Returns a closure which handles Mojo connection errors tied to Account
  // Manager.
  base::OnceClosure MojoDisconnectionClosure() {
    return base::BindOnce(&AccessTokenFetcher::OnMojoError,
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
      OnMojoError();
    }
  }

  void FetchAccessToken(
      mojo::PendingRemote<crosapi::mojom::AccessTokenFetcher> pending_remote) {
    access_token_fetcher_.Bind(std::move(pending_remote));
    access_token_fetcher_->Start(
        scopes_, base::BindOnce(&AccessTokenFetcher::OnAccessTokenFetchComplete,
                                weak_factory_.GetWeakPtr()));
  }

  void OnAccessTokenFetchComplete(crosapi::mojom::AccessTokenResultPtr result) {
    DCHECK(is_request_pending_);
    is_request_pending_ = false;

    if (result->is_error()) {
      absl::optional<GoogleServiceAuthError> maybe_error =
          account_manager::FromMojoGoogleServiceAuthError(result->get_error());

      if (!maybe_error.has_value()) {
        LOG(ERROR) << "Unable to parse error result of access token fetch: "
                   << result->get_error()->state;
        FireOnGetTokenFailure(GoogleServiceAuthError(
            GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));
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

  void OnMojoError() {
    if (!is_request_pending_)
      return;

    CancelRequest();
    FireOnGetTokenFailure(
        GoogleServiceAuthError::FromServiceError("Mojo pipe disconnected"));
  }

  AccountManagerFacadeImpl* const account_manager_facade_impl_;
  const account_manager::AccountKey account_key_;
  const std::string oauth_consumer_name_;

  bool are_token_requests_allowed_ = false;
  bool is_request_pending_ = false;
  std::vector<std::string> scopes_;
  mojo::Remote<crosapi::mojom::AccessTokenFetcher> access_token_fetcher_;

  base::WeakPtrFactory<AccessTokenFetcher> weak_factory_{this};
};

AccountManagerFacadeImpl::AccountManagerFacadeImpl(
    mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
    uint32_t remote_version,
    AccountManager* account_manager_for_tests,
    base::OnceClosure init_finished)
    : remote_version_(remote_version),
      account_manager_remote_(std::move(account_manager_remote)),
      account_manager_for_tests_(account_manager_for_tests) {
  DCHECK(init_finished);
  initialization_callbacks_.emplace_back(std::move(init_finished));

  if (!account_manager_remote_ ||
      remote_version_ < RemoteMinVersions::kGetAccountsMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_
                 << ", expected: " << RemoteMinVersions::kGetAccountsMinVersion
                 << ". Account consistency will be disabled";
    FinishInitSequenceIfNotAlreadyFinished();
    return;
  }

  account_manager_remote_.set_disconnect_handler(base::BindOnce(
      &AccountManagerFacadeImpl::OnMojoError, weak_factory_.GetWeakPtr()));
  account_manager_remote_->AddObserver(
      base::BindOnce(&AccountManagerFacadeImpl::OnReceiverReceived,
                     weak_factory_.GetWeakPtr()));
}

AccountManagerFacadeImpl::~AccountManagerFacadeImpl() = default;

void AccountManagerFacadeImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountManagerFacadeImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AccountManagerFacadeImpl::GetAccounts(
    base::OnceCallback<void(const std::vector<Account>&)> callback) {
  // Record the status of the mojo connection, to get more information about
  // https://crbug.com/1287297
  FacadeMojoStatus mojo_status = FacadeMojoStatus::kOk;
  if (!account_manager_remote_)
    mojo_status = FacadeMojoStatus::kNoRemote;
  else if (remote_version_ < RemoteMinVersions::kGetAccountsMinVersion)
    mojo_status = FacadeMojoStatus::kVersionMismatch;
  else if (!is_initialized_)
    mojo_status = FacadeMojoStatus::kUninitialized;
  base::UmaHistogramEnumeration(kGetAccountsMojoStatus, mojo_status);

  if (!account_manager_remote_ ||
      remote_version_ < RemoteMinVersions::kGetAccountsMinVersion) {
    // Remote side is disconnected or doesn't support GetAccounts. Do not return
    // an empty list as that may cause Lacros to delete user profiles.
    // TODO(https://crbug.com/1287297): Try to reconnect, or return an error.
    return;
  }
  RunAfterInitializationSequence(
      base::BindOnce(&AccountManagerFacadeImpl::GetAccountsInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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

void AccountManagerFacadeImpl::ShowAddAccountDialog(
    AccountAdditionSource source) {
  ShowAddAccountDialog(source, base::DoNothing());
}

void AccountManagerFacadeImpl::ShowAddAccountDialog(
    AccountAdditionSource source,
    base::OnceCallback<
        void(const account_manager::AccountAdditionResult& result)> callback) {
  if (!account_manager_remote_ ||
      remote_version_ < RemoteMinVersions::kShowAddAccountDialogMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_ << ", expected: "
                 << RemoteMinVersions::kShowAddAccountDialogMinVersion
                 << " for ShowAddAccountDialog.";
    FinishAddAccount(std::move(callback),
                     account_manager::AccountAdditionResult::FromStatus(
                         account_manager::AccountAdditionResult::Status::
                             kUnexpectedResponse));
    return;
  }

  base::UmaHistogramEnumeration(kAccountAdditionSource, source);

  crosapi::mojom::AccountAdditionOptionsPtr options =
      crosapi::mojom::AccountAdditionOptions::New();
  options->is_available_in_arc = GetIsAvailableInArcBySource(source);
  options->show_arc_availability_picker =
      (source == AccountManagerFacade::AccountAdditionSource::kArc);

  account_manager_remote_->ShowAddAccountDialog(
      std::move(options),
      base::BindOnce(&AccountManagerFacadeImpl::OnShowAddAccountDialogFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AccountManagerFacadeImpl::ShowReauthAccountDialog(
    AccountAdditionSource source,
    const std::string& email) {
  if (!account_manager_remote_ ||
      remote_version_ < RemoteMinVersions::kShowReauthAccountDialogMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_ << ", expected: "
                 << RemoteMinVersions::kShowReauthAccountDialogMinVersion
                 << " for ShowReauthAccountDialog.";
    return;
  }

  base::UmaHistogramEnumeration(kAccountAdditionSource, source);

  account_manager_remote_->ShowReauthAccountDialog(email, base::DoNothing());
}

void AccountManagerFacadeImpl::ShowManageAccountsSettings() {
  if (!account_manager_remote_ ||
      remote_version_ <
          RemoteMinVersions::kShowManageAccountsSettingsMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_ << ", expected: "
                 << RemoteMinVersions::kShowManageAccountsSettingsMinVersion
                 << " for ShowManageAccountsSettings.";
    return;
  }

  account_manager_remote_->ShowManageAccountsSettings();
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountManagerFacadeImpl::CreateAccessTokenFetcher(
    const AccountKey& account,
    const std::string& oauth_consumer_name,
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
      /*account_manager_facade_impl=*/this, account, oauth_consumer_name,
      consumer);
  RunAfterInitializationSequence(access_token_fetcher->UnblockTokenRequest());
  RunOnMojoDisconnection(access_token_fetcher->MojoDisconnectionClosure());
  return std::move(access_token_fetcher);
}

void AccountManagerFacadeImpl::UpsertAccountForTesting(
    const Account& account,
    const std::string& token_value) {
  account_manager_for_tests_->UpsertAccount(account.key, account.raw_email,
                                            token_value);
}

void AccountManagerFacadeImpl::RemoveAccountForTesting(
    const AccountKey& account) {
  account_manager_for_tests_->RemoveAccount(account);
}

// static
std::string AccountManagerFacadeImpl::
    GetAccountAdditionResultStatusHistogramNameForTesting() {
  return kAccountAdditionResultStatus;
}

// static
std::string
AccountManagerFacadeImpl::GetAccountsMojoStatusHistogramNameForTesting() {
  return kGetAccountsMojoStatus;
}

void AccountManagerFacadeImpl::OnReceiverReceived(
    mojo::PendingReceiver<AccountManagerObserver> receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>(
          this, std::move(receiver));
  // At this point (`receiver_` exists), we are subscribed to Account Manager.

  FinishInitSequenceIfNotAlreadyFinished();
}

void AccountManagerFacadeImpl::OnShowAddAccountDialogFinished(
    base::OnceCallback<
        void(const account_manager::AccountAdditionResult& result)> callback,
    crosapi::mojom::AccountAdditionResultPtr mojo_result) {
  absl::optional<account_manager::AccountAdditionResult> result =
      account_manager::FromMojoAccountAdditionResult(mojo_result);
  if (!result.has_value()) {
    FinishAddAccount(std::move(callback),
                     account_manager::AccountAdditionResult::FromStatus(
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
  base::UmaHistogramEnumeration(kAccountAdditionResultStatus, result.status());
  std::move(callback).Run(result);
}

void AccountManagerFacadeImpl::OnTokenUpserted(
    crosapi::mojom::AccountPtr account) {
  absl::optional<Account> maybe_account = FromMojoAccount(account);
  if (!maybe_account) {
    LOG(WARNING) << "Can't unmarshal account of type: "
                 << account->key->account_type;
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnAccountUpserted(maybe_account.value());
  }
}

void AccountManagerFacadeImpl::OnAccountRemoved(
    crosapi::mojom::AccountPtr account) {
  absl::optional<Account> maybe_account = FromMojoAccount(account);
  if (!maybe_account) {
    LOG(WARNING) << "Can't unmarshal account of type: "
                 << account->key->account_type;
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnAccountRemoved(maybe_account.value());
  }
}

void AccountManagerFacadeImpl::GetAccountsInternal(
    base::OnceCallback<void(const std::vector<Account>&)> callback) {
  account_manager_remote_->GetAccounts(
      base::BindOnce(&UnmarshalAccounts, std::move(callback)));
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

void AccountManagerFacadeImpl::RunOnMojoDisconnection(
    base::OnceClosure closure) {
  if (!account_manager_remote_) {
    std::move(closure).Run();
    return;
  }
  mojo_disconnection_handlers_.emplace_back(std::move(closure));
}

void AccountManagerFacadeImpl::OnMojoError() {
  LOG(ERROR) << "Account Manager disconnected";
  for (auto& cb : mojo_disconnection_handlers_) {
    std::move(cb).Run();
  }
  mojo_disconnection_handlers_.clear();
  account_manager_remote_.reset();
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
