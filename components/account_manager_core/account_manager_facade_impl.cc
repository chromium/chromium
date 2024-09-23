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
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"

namespace account_manager {

namespace {

using RemoteMinVersions = crosapi::mojom::AccountManager::MethodMinVersions;

// UMA histogram names.
const char kAccountUpsertionResultStatus[] =
    "AccountManager.AccountUpsertionResultStatus";
const char kGetAccountsMojoStatus[] =
    "AccountManager.FacadeGetAccountsMojoStatus";
const char kMojoDisconnectionsAccountManagerRemote[] =
    "AccountManager.MojoDisconnections.AccountManagerRemote";
const char kMojoDisconnectionsAccountManagerObserverReceiver[] =
    "AccountManager.MojoDisconnections.AccountManagerObserverReceiver";
const char kMojoDisconnectionsAccountManagerAccessTokenFetcherRemote[] =
    "AccountManager.MojoDisconnections.AccessTokenFetcherRemote";

void UnmarshalAccounts(
    base::OnceCallback<void(const std::vector<Account>&)> callback,
    std::vector<crosapi::mojom::AccountPtr> mojo_accounts) {
  std::vector<Account> accounts;
  for (const auto& mojo_account : mojo_accounts) {
    std::optional<Account> maybe_account = FromMojoAccount(mojo_account);
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
    case AccountManagerFacade::AccountAdditionSource::
        kAvatarBubbleTurnOnSyncAddAccount:
    case AccountManagerFacade::AccountAdditionSource::
        kChromeExtensionAddAccount:
    case AccountManagerFacade::AccountAdditionSource::
        kChromeSyncPromoAddAccount:
    case AccountManagerFacade::AccountAdditionSource::
        kChromeSettingsTurnOnSyncButton:
    case AccountManagerFacade::AccountAdditionSource::kChromeMenuTurnOnSync:
    case AccountManagerFacade::AccountAdditionSource::
        kChromeSigninPromoAddAccount:
      return false;
    // These are reauthentication cases. ARC visibility shouldn't change for
    // reauthentication.
    case AccountManagerFacade::AccountAdditionSource::kContentAreaReauth:
    case AccountManagerFacade::AccountAdditionSource::
        kSettingsReauthAccountButton:
    case AccountManagerFacade::AccountAdditionSource::
        kAvatarBubbleReauthAccountButton:
    case AccountManagerFacade::AccountAdditionSource::kChromeExtensionReauth:
    case AccountManagerFacade::AccountAdditionSource::kChromeSyncPromoReauth:
    case AccountManagerFacade::AccountAdditionSource::
        kChromeOSProjectorAppReauth:
    case AccountManagerFacade::AccountAdditionSource::
        kChromeSettingsReauthAccountButton:
      NOTREACHED_IN_MIGRATION();
      return false;
    // Unused enums that cannot be deleted.
    case AccountManagerFacade::AccountAdditionSource::kPrintPreviewDialogUnused:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
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
    base::WeakPtr<AccountManager> account_manager_for_tests,
    base::OnceClosure init_finished)
    : remote_version_(remote_version),
      account_manager_remote_(std::move(account_manager_remote)),
      account_manager_for_tests_(std::move(account_manager_for_tests)) {
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
    // TODO(crbug.com/40211181): Try to reconnect, or return an error.
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
        void(const account_manager::AccountUpsertionResult& result)> callback) {
  if (!account_manager_remote_) {
    LOG(WARNING) << "Account Manager remote disconnected";
    FinishUpsertAccount(
        std::move(callback),
        AccountUpsertionResult::FromStatus(
            AccountUpsertionResult::Status::kMojoRemoteDisconnected));
    return;
  }

  if (remote_version_ < RemoteMinVersions::kShowAddAccountDialogMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_ << ", expected: "
                 << RemoteMinVersions::kShowAddAccountDialogMinVersion
                 << " for ShowAddAccountDialog.";
    FinishUpsertAccount(
        std::move(callback),
        AccountUpsertionResult::FromStatus(
            AccountUpsertionResult::Status::kIncompatibleMojoVersions));
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
      base::BindOnce(&AccountManagerFacadeImpl::OnSigninDialogActionFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AccountManagerFacadeImpl::ShowReauthAccountDialog(
    AccountAdditionSource source,
    const std::string& email,
    base::OnceCallback<
        void(const account_manager::AccountUpsertionResult& result)> callback) {
  if (!account_manager_remote_) {
    LOG(WARNING) << "Account Manager remote disconnected";
    FinishUpsertAccount(
        std::move(callback),
        AccountUpsertionResult::FromStatus(
            AccountUpsertionResult::Status::kMojoRemoteDisconnected));
    return;
  }

  if (remote_version_ < RemoteMinVersions::kShowReauthAccountDialogMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_ << ", expected: "
                 << RemoteMinVersions::kShowReauthAccountDialogMinVersion
                 << " for ShowReauthAccountDialog.";
    FinishUpsertAccount(
        std::move(callback),
        AccountUpsertionResult::FromStatus(
            AccountUpsertionResult::Status::kIncompatibleMojoVersions));
    return;
  }

  base::UmaHistogramEnumeration(kAccountAdditionSource, source);

  account_manager_remote_->ShowReauthAccountDialog(
      email,
      base::BindOnce(&AccountManagerFacadeImpl::OnSigninDialogActionFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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
  if (!account_manager_remote_ ||
      remote_version_ < RemoteMinVersions::kReportAuthErrorMinVersion) {
    LOG(WARNING) << "Found remote at: " << remote_version_ << ", expected: "
                 << RemoteMinVersions::kReportAuthErrorMinVersion
                 << " for ReportAuthError.";
    return;
  }

  account_manager_remote_->ReportAuthError(ToMojoAccountKey(account),
                                           ToMojoGoogleServiceAuthError(error));
}

void AccountManagerFacadeImpl::UpsertAccountForTesting(
    const Account& account,
    const std::string& token_value) {
  CHECK(account_manager_for_tests_);
  account_manager_for_tests_->UpsertAccount(account.key, account.raw_email,
                                            token_value);
}

void AccountManagerFacadeImpl::RemoveAccountForTesting(
    const AccountKey& account) {
  CHECK(account_manager_for_tests_);
  account_manager_for_tests_->RemoveAccount(account);
}

// static
std::string AccountManagerFacadeImpl::
    GetAccountUpsertionResultStatusHistogramNameForTesting() {
  return kAccountUpsertionResultStatus;
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

  receiver_->set_disconnect_handler(base::BindOnce(
      &AccountManagerFacadeImpl::OnAccountManagerObserverReceiverDisconnected,
      weak_factory_.GetWeakPtr()));

  FinishInitSequenceIfNotAlreadyFinished();
}

void AccountManagerFacadeImpl::OnSigninDialogActionFinished(
    base::OnceCallback<
        void(const account_manager::AccountUpsertionResult& result)> callback,
    crosapi::mojom::AccountUpsertionResultPtr mojo_result) {
  std::optional<account_manager::AccountUpsertionResult> result =
      account_manager::FromMojoAccountUpsertionResult(mojo_result);
  if (!result.has_value()) {
    FinishUpsertAccount(
        std::move(callback),
        AccountUpsertionResult::FromStatus(
            AccountUpsertionResult::Status::kUnexpectedResponse));
    return;
  }
  FinishUpsertAccount(std::move(callback), result.value());
}

void AccountManagerFacadeImpl::FinishUpsertAccount(
    base::OnceCallback<
        void(const account_manager::AccountUpsertionResult& result)> callback,
    const account_manager::AccountUpsertionResult& result) {
  base::UmaHistogramEnumeration(kAccountUpsertionResultStatus, result.status());
  std::move(callback).Run(result);
}

void AccountManagerFacadeImpl::OnTokenUpserted(
    crosapi::mojom::AccountPtr account) {
  std::optional<Account> maybe_account = FromMojoAccount(account);
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
  std::optional<Account> maybe_account = FromMojoAccount(account);
  if (!maybe_account) {
    LOG(WARNING) << "Can't unmarshal account of type: "
                 << account->key->account_type;
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnAccountRemoved(maybe_account.value());
  }
}

void AccountManagerFacadeImpl::OnAuthErrorChanged(
    crosapi::mojom::AccountKeyPtr account,
    crosapi::mojom::GoogleServiceAuthErrorPtr error) {
  std::optional<AccountKey> maybe_account_key = FromMojoAccountKey(account);
  if (!maybe_account_key) {
    LOG(WARNING) << "Can't unmarshal account key of type: "
                 << account->account_type;
    return;
  }

  std::optional<GoogleServiceAuthError> maybe_error =
      FromMojoGoogleServiceAuthError(error);
  if (!maybe_error) {
    LOG(WARNING) << "Can't unmarshal error with state: " << error->state;
    return;
  }

  for (auto& observer : observer_list_) {
    observer.OnAuthErrorChanged(maybe_account_key.value(), maybe_error.value());
  }
}

void AccountManagerFacadeImpl::OnSigninDialogClosed() {
  for (auto& observer : observer_list_) {
    observer.OnSigninDialogClosed();
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
