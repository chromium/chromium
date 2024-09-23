// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_chromeos.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/account_manager_core/account.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

namespace {

// Maps crOS Account Manager |account_keys| to the account id representation
// used by the OAuth token service chain. |account_keys| can safely contain Gaia
// and non-Gaia accounts. Non-Gaia accounts will be filtered out.
// |account_keys| is the set of accounts that need to be translated.
// |account_tracker_service| is an unowned pointer.
std::vector<CoreAccountId> GetOAuthAccountIdsFromAccountKeys(
    const std::set<account_manager::AccountKey>& account_keys,
    const AccountTrackerService* const account_tracker_service) {
  std::vector<CoreAccountId> accounts;
  for (auto& account_key : account_keys) {
    if (account_key.account_type() != account_manager::AccountType::kGaia) {
      continue;
    }

    CoreAccountId account_id =
        account_tracker_service
            ->FindAccountInfoByGaiaId(account_key.id() /* gaia_id */)
            .account_id;
    DCHECK(!account_id.empty());
    accounts.emplace_back(account_id);
  }

  return accounts;
}

// Helper class to request persistent errors for multiple accounts.
// See `GetErrorsForMultipleAccounts` for details.
class PersistentErrorsHelper : public base::RefCounted<PersistentErrorsHelper> {
 public:
  using AccountToErrorMap =
      std::map<account_manager::AccountKey, GoogleServiceAuthError>;

  // Do not instantiate manually - use `GetErrorsForMultipleAccounts` instead.
  // Made public for MakeRefCounted only.
  PersistentErrorsHelper(
      int outstanding_requests,
      base::OnceCallback<void(const AccountToErrorMap&)> callback)
      : outstanding_requests_(outstanding_requests),
        callback_(std::move(callback)) {}

  PersistentErrorsHelper(const PersistentErrorsHelper&) = delete;
  PersistentErrorsHelper& operator=(const PersistentErrorsHelper&) = delete;

  // Asynchronously gets persistent errors for `accounts` from
  // `account_manager_facade` and passes them to `callback`.
  //
  // Note: If `account_manager_facade` doesn't call one of the callbacks passed
  // to `AccountManagerFacade::GetPersistentErrorForAccount` (for example, if
  // the Mojo connection is interrupted), then `callback` will not be invoked.
  static void GetErrorsForMultipleAccounts(
      account_manager::AccountManagerFacade* account_manager_facade,
      const std::vector<account_manager::Account>& accounts,
      base::OnceCallback<void(const AccountToErrorMap&)> callback) {
    DCHECK(account_manager_facade);
    if (accounts.empty()) {
      // No accounts to get error status for, run callback immediately.
      std::move(callback).Run(
          std::map<account_manager::AccountKey, GoogleServiceAuthError>());
      return;
    }

    // The ownership of this object is shared between callbacks passed to
    // `AccountManagerFacade::GetPersistentErrorForAccount`.
    scoped_refptr<PersistentErrorsHelper> shared_state =
        base::MakeRefCounted<PersistentErrorsHelper>(accounts.size(),
                                                     std::move(callback));
    // Request error statuses for all accounts.
    for (const auto& account : accounts) {
      account_manager_facade->GetPersistentErrorForAccount(
          account.key,
          base::BindOnce(
              &PersistentErrorsHelper::OnGetPersistentErrorForAccount,
              shared_state, account.key));
    }
  }

 private:
  friend base::RefCounted<PersistentErrorsHelper>;

  ~PersistentErrorsHelper() = default;

  void OnGetPersistentErrorForAccount(
      const account_manager::AccountKey& account,
      const GoogleServiceAuthError& error) {
    DCHECK_GT(outstanding_requests_, 0);
    persistent_errors_.emplace(account, error);
    if (--outstanding_requests_ == 0)
      std::move(callback_).Run(persistent_errors_);
  }

  AccountToErrorMap persistent_errors_;
  int outstanding_requests_;
  base::OnceCallback<void(const AccountToErrorMap&)> callback_;
};

}  // namespace

ProfileOAuth2TokenServiceDelegateChromeOS::
    ProfileOAuth2TokenServiceDelegateChromeOS(
        SigninClient* signin_client,
        AccountTrackerService* account_tracker_service,
        network::NetworkConnectionTracker* network_connection_tracker,
        account_manager::AccountManagerFacade* account_manager_facade,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        bool delete_signin_cookies_on_exit,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
        bool is_regular_profile)
    : ProfileOAuth2TokenServiceDelegate(/*use_backoff=*/true),
      signin_client_(signin_client),
      account_tracker_service_(account_tracker_service),
      network_connection_tracker_(network_connection_tracker),
      account_manager_facade_(account_manager_facade),
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      delete_signin_cookies_on_exit_(delete_signin_cookies_on_exit),
#endif
      is_regular_profile_(is_regular_profile),
      weak_factory_(this) {
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

ProfileOAuth2TokenServiceDelegateChromeOS::
    ~ProfileOAuth2TokenServiceDelegateChromeOS() {
  account_manager_facade_->RemoveObserver(this);
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileOAuth2TokenServiceDelegateChromeOS::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      load_credentials_state());

  ValidateAccountId(account_id);

  // Check if we need to reject the request.
  // We will reject the request if we are facing a persistent error for this
  // account.
  GoogleServiceAuthError error = GetAuthError(account_id);
  if (error.IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << error.state();
    // |ProfileOAuth2TokenService| will manage the lifetime of this pointer.
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(consumer,
                                                                    error);
  }
  // Or when we need to backoff.
  if (BackoffEntry()->ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << BackOffError().state();
    // |ProfileOAuth2TokenService| will manage the lifetime of this pointer.
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer, BackOffError());
  }

  return account_manager_facade_->CreateAccessTokenFetcher(
      account_manager::AccountKey{
          account_tracker_service_->GetAccountInfo(account_id).gaia,
          account_manager::AccountType::kGaia} /* account_key */,
      consumer);
}

// Note: This method should use the same logic for filtering accounts as
// |GetAccounts|. See crbug.com/919793 for details. At the time of writing,
// both |GetAccounts| and |RefreshTokenIsAvailable| use
// |GetOAuthAccountIdsFromAccountKeys|.
bool ProfileOAuth2TokenServiceDelegateChromeOS::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  if (load_credentials_state() !=
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
    return false;
  }

  // We intentionally do NOT check if the refresh token associated with
  // |account_id| is valid or not. See crbug.com/919793 for details.
  return base::Contains(GetOAuthAccountIdsFromAccountKeys(
                            account_keys_, account_tracker_service_),
                        account_id);
}

// Note: This method should use the same logic for filtering accounts as
// |RefreshTokenIsAvailable|. See crbug.com/919793 for details. At the time of
// writing, both |GetAccounts| and |RefreshTokenIsAvailable| use
// |GetOAuthAccountIdsFromAccountKeys|.
std::vector<CoreAccountId>
ProfileOAuth2TokenServiceDelegateChromeOS::GetAccounts() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |GetAccounts| intentionally does not care about the state of
  // |load_credentials_state|. See crbug.com/919793 and crbug.com/900590 for
  // details.

  return GetOAuthAccountIdsFromAccountKeys(account_keys_,
                                           account_tracker_service_);
}

void ProfileOAuth2TokenServiceDelegateChromeOS::LoadCredentialsInternal(
    const CoreAccountId& primary_account_id,
    bool is_syncing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (load_credentials_state() !=
      signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED) {
    return;
  }
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS);

  if (!is_regular_profile_) {
    // |LoadCredentials| needs to complete successfully for a successful Profile
    // initialization, but for Signin Profile and Lock Screen Profile this is a
    // no-op: they do not and must not have a working Account Manager available
    // to them. Note: They do have access to an Account Manager instance, but
    // that instance is never set up (|AccountManager::Initialize|). Also, see:
    // - http://crbug.com/891818
    // - https://crbug.com/996615 and |GetURLLoaderFactory|.
    set_load_credentials_state(
        signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
    FireRefreshTokensLoaded();
    return;
  }

  DCHECK(account_manager_facade_);
  account_manager_facade_->AddObserver(this);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, signin cookies can only be cleared for non-syncing
  // secondary profiles, because:
  // - the main profile cannot be signed out,
  // - clearing cookie does not turn sync off
  // Additionally, there is no way for Chrome to "invalidate" a token. In
  // particular, the "sync paused" state does not exist.
  bool revoke_all_tokens =
      delete_signin_cookies_on_exit_ && !is_syncing &&
      !signin_client_->GetInitialPrimaryAccount().has_value();
  if (revoke_all_tokens)
    RevokeAllCredentials();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  account_manager_facade_->GetAccounts(
      base::BindOnce(&ProfileOAuth2TokenServiceDelegateChromeOS::OnGetAccounts,
                     weak_factory_.GetWeakPtr()));
}

void ProfileOAuth2TokenServiceDelegateChromeOS::UpdateCredentialsInternal(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NOTREACHED_IN_MIGRATION()
      << "If you're seeing this error in a browser_test, consider "
         "disabling the test while we set up the testing "
         "infrastructure to talk to Ash in a browser_test. Also, please add a "
         "comment / CL ref. to crbug.com/1197201 if you disable your test.";
  // TODO(sinhak): We need a way to write accounts to Account Manager in
  // browser_tests and lacros_chrome_browsertests. For browser_tests, the
  // solution may be to build Account Manager in Lacros. For
  // lacros_chrome_browsertests, we will need to talk to EngProd.
#else
  // UpdateCredentials should not be called on Chrome OS. Credentials should be
  // updated through Chrome OS Account Manager.
  NOTREACHED_IN_MIGRATION();
#endif
}

scoped_refptr<network::SharedURLLoaderFactory>
ProfileOAuth2TokenServiceDelegateChromeOS::GetURLLoaderFactory() const {
    return nullptr;
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnGetAccounts(
    const std::vector<account_manager::Account>& accounts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This callback should only be triggered during |LoadCredentials|, which
  // implies that |load_credentials_state())| should in
  // |LOAD_CREDENTIALS_IN_PROGRESS| state.
  DCHECK_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            load_credentials_state());
  std::vector<account_manager::Account> gaia_accounts;
  for (const auto& account : accounts) {
    pending_accounts_.emplace(account.key, account);
    if (account.key.account_type() == account_manager::AccountType::kGaia) {
      gaia_accounts.emplace_back(account);
    }
  }
  PersistentErrorsHelper::GetErrorsForMultipleAccounts(
      account_manager_facade_, gaia_accounts,
      base::BindOnce(
          &ProfileOAuth2TokenServiceDelegateChromeOS::FinishLoadingCredentials,
          weak_factory_.GetWeakPtr(), std::move(accounts)));
}

void ProfileOAuth2TokenServiceDelegateChromeOS::FinishLoadingCredentials(
    const std::vector<account_manager::Account>& accounts,
    const std::map<account_manager::AccountKey, GoogleServiceAuthError>&
        persistent_errors) {
  DCHECK_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            load_credentials_state());
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  // The typical order of |ProfileOAuth2TokenServiceObserver| callbacks is:
  // 1. OnRefreshTokenAvailable
  // 2. OnEndBatchChanges
  // 3. OnRefreshTokensLoaded
  {
    ScopedBatchChange batch(this);
    for (const auto& account : accounts) {
      auto it = persistent_errors.find(account.key);
      if (it != persistent_errors.end()) {
        FinishAddingPendingAccount(account, it->second);
      } else {
        DCHECK_NE(account.key.account_type(),
                  account_manager::AccountType::kGaia);
        FinishAddingPendingAccount(account,
                                   GoogleServiceAuthError::AuthErrorNone());
      }
    }
  }
  FireRefreshTokensLoaded();

  // The first batch of OnRefreshTokenAvailable calls should contain the list
  // of accounts obtained from `GetAccounts`, even if there are
  // `OnAccountUpserted` notification that were received right after calling
  // `GetAccounts`. To avoid this `front running`, `OnAccountUpserted` won't
  // process notifications that arrive before credentials are loaded,
  // queueing them in `pending_accounts_` instead. Start processing these
  // requests now.
  //
  // Make a copy of `pending_accounts_`, since `OnAccountUpserted` might modify
  // that collection.
  std::map<account_manager::AccountKey, account_manager::Account>
      pending_accounts(pending_accounts_);
  for (const auto& pending_account : pending_accounts) {
    OnAccountUpserted(pending_account.second);
  }
}

void ProfileOAuth2TokenServiceDelegateChromeOS::FinishAddingPendingAccount(
    const account_manager::Account& account,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      load_credentials_state());
  auto it = pending_accounts_.find(account.key);
  if (it == pending_accounts_.end()) {
    // The account was removed using |OnAccountRemoved| before we finished
    // adding it.
    return;
  }
  pending_accounts_.erase(it);
  account_keys_.insert(account.key);

  if (account.key.account_type() != account_manager::AccountType::kGaia) {
    return;
  }

  // All Gaia accounts in Chrome OS Account Manager must have an email
  // associated with them (https://crbug.com/933307).
  DCHECK(!account.raw_email.empty());
  CoreAccountId account_id = account_tracker_service_->SeedAccountInfo(
      account.key.id() /* gaia_id */, account.raw_email);
  DCHECK(!account_id.empty());

  // Call the parent method - which will not report the error back to
  // `AccountManagerFacade` and result in this instance getting notified again -
  // unlike `ProfileOAuth2TokenServiceDelegateChromeOS::UpdateAuthError`.
  // Additionally, don't call `FireAuthErrorChanged`
  // (/*fire_auth_error_changed=*/false), since we call it at the end of this
  // function.
  ProfileOAuth2TokenServiceDelegate::UpdateAuthError(
      account_id, error, /*fire_auth_error_changed=*/false);

  FireRefreshTokenAvailable(account_id);
  // See |ProfileOAuth2TokenServiceObserver::OnAuthErrorChanged|.
  // |OnAuthErrorChanged| must be always called after
  // |OnRefreshTokenAvailable|, when refresh token is updated.
  FireAuthErrorChanged(account_id, error);
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnAccountUpserted(
    const account_manager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_accounts_.emplace(account.key, account);

  if (load_credentials_state() !=
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
    // Haven't finished loading credentials yet, postpone adding the account.
    // `FinishLoadingCredentials` will continue adding this account when the
    // initial list of account has been processed.
    return;
  }

  if (account.key.account_type() != account_manager::AccountType::kGaia) {
    // Don't request pending account status for non-Gaia accounts.
    FinishAddingPendingAccount(account,
                               GoogleServiceAuthError::AuthErrorNone());
    return;
  }

  account_manager_facade_->GetPersistentErrorForAccount(
      account.key, base::BindOnce(&ProfileOAuth2TokenServiceDelegateChromeOS::
                                      FinishAddingPendingAccount,
                                  weak_factory_.GetWeakPtr(), account));
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnAccountRemoved(
    const account_manager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto pending_it = pending_accounts_.find(account.key);
  if (pending_it != pending_accounts_.end()) {
    // The delegate hasn't yet finished processing `OnAccountUpserted` call for
    // this account. Remove it from `pending_accounts_` to let
    // `FinishAddingPendingAccount` know that the account was removed.
    // Do not return yet, as the aforementioned `OnAccountUpserted` call could
    // be for an already known account. In this case, the account should be
    // removed and observers notified accordingly.
    pending_accounts_.erase(pending_it);
  }

  auto it = account_keys_.find(account.key);
  if (it == account_keys_.end()) {
    return;
  }
  account_keys_.erase(it);

  if (account.key.account_type() != account_manager::AccountType::kGaia) {
    return;
  }
  CoreAccountId account_id =
      account_tracker_service_
          ->FindAccountInfoByGaiaId(account.key.id() /* gaia_id */)
          .account_id;
  DCHECK(!account_id.empty());
  ClearAuthError(account_id);

  // ProfileOAuth2TokenService will clear its cache for |account_id| when this
  // is called. See |ProfileOAuth2TokenService::OnRefreshTokenRevoked|.
  FireRefreshTokenRevoked(account_id);
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnAuthErrorChanged(
    const account_manager::AccountKey& account,
    const GoogleServiceAuthError& error) {
  if (account_keys_.find(account) == account_keys_.end()) {
    LOG(ERROR) << "Received error update for unknown account";
    return;
  }
  CoreAccountId account_id =
      account_tracker_service_->FindAccountInfoByGaiaId(account.id())
          .account_id;

  if (error == GetAuthError(account_id)) {
    // Nothing to do if the error is already known.
    return;
  }

  // Call the parent method - which will not report the error back to
  // `AccountManagerFacade` and result in this instance getting notified again -
  // unlike `ProfileOAuth2TokenServiceDelegateChromeOS::UpdateAuthError`.
  ProfileOAuth2TokenServiceDelegate::UpdateAuthError(account_id, error);
}

void ProfileOAuth2TokenServiceDelegateChromeOS::RevokeCredentialsInternal(
    const CoreAccountId& account_id) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ScopedBatchChange batch(this);
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.IsEmpty());
  signin_client_->RemoveAccount(account_manager::AccountKey{
      account_info.gaia, account_manager::AccountType::kGaia});
  ClearAuthError(account_id);
#else
  // Signing out of Chrome is not possible on Chrome OS Ash / Lacros.
  NOTREACHED_IN_MIGRATION();
#endif
}

void ProfileOAuth2TokenServiceDelegateChromeOS::RevokeAllCredentialsInternal(
    signin_metrics::SourceForRefreshTokenOperation source) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  DCHECK(!signin_client_->GetInitialPrimaryAccount().has_value());
  ScopedBatchChange batch(this);
  signin_client_->RemoveAllAccounts();
#else
  // Signing out of Chrome is not possible on Chrome OS Ash.
  NOTREACHED_IN_MIGRATION();
#endif
}

void ProfileOAuth2TokenServiceDelegateChromeOS::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error,
    bool fire_auth_error_changed) {
  ProfileOAuth2TokenServiceDelegate::UpdateAuthError(account_id, error,
                                                     fire_auth_error_changed);
  if (!RefreshTokenIsAvailable(account_id)) {
    // Account has been removed.
    DCHECK_EQ(error, GoogleServiceAuthError(
                         GoogleServiceAuthError::USER_NOT_SIGNED_UP));
    return;
  }

  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.IsEmpty());
  account_manager_facade_->ReportAuthError(
      account_manager::AccountKey{account_info.gaia,
                                  account_manager::AccountType::kGaia},
      error);
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  ResetBackOffEntry();
}

}  // namespace signin
