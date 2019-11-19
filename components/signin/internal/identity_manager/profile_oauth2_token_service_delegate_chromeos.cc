// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_chromeos.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

namespace {

// Values used from |MutableProfileOAuth2TokenServiceDelegate|.
const net::BackoffEntry::Policy kBackoffPolicy = {
    0 /* int num_errors_to_ignore */,

    1000 /* int initial_delay_ms */,

    2.0 /* double multiply_factor */,

    0.2 /* double jitter_factor */,

    15 * 60 * 1000 /* int64_t maximum_backoff_ms */,

    -1 /* int64_t entry_lifetime_ms */,

    false /* bool always_use_initial_delay */,
};

// Maps crOS Account Manager |account_keys| to the account id representation
// used by the OAuth token service chain. |account_keys| can safely contain Gaia
// and non-Gaia accounts. Non-Gaia accounts will be filtered out.
// |account_keys| is the set of accounts that need to be translated.
// |account_tracker_service| is an unowned pointer.
std::vector<CoreAccountId> GetOAuthAccountIdsFromAccountKeys(
    const std::set<chromeos::AccountManager::AccountKey>& account_keys,
    const AccountTrackerService* const account_tracker_service) {
  std::vector<CoreAccountId> accounts;
  for (auto& account_key : account_keys) {
    if (account_key.account_type !=
        chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
      continue;
    }

    CoreAccountId account_id =
        account_tracker_service
            ->FindAccountInfoByGaiaId(account_key.id /* gaia_id */)
            .account_id;
    DCHECK(!account_id.empty());
    accounts.emplace_back(account_id);
  }

  return accounts;
}

}  // namespace

ProfileOAuth2TokenServiceDelegateChromeOS::
    ProfileOAuth2TokenServiceDelegateChromeOS(
        AccountTrackerService* account_tracker_service,
        network::NetworkConnectionTracker* network_connection_tracker,
        chromeos::AccountManager* account_manager,
        bool is_regular_profile)
    : account_tracker_service_(account_tracker_service),
      network_connection_tracker_(network_connection_tracker),
      account_manager_(account_manager),
      backoff_entry_(&kBackoffPolicy),
      backoff_error_(GoogleServiceAuthError::NONE),
      is_regular_profile_(is_regular_profile),
      weak_factory_(this) {
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

ProfileOAuth2TokenServiceDelegateChromeOS::
    ~ProfileOAuth2TokenServiceDelegateChromeOS() {
  account_manager_->RemoveObserver(this);
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileOAuth2TokenServiceDelegateChromeOS::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      load_credentials_state());

  ValidateAccountId(account_id);

  // Check if we need to reject the request.
  // We will reject the request if we are facing a persistent error for this
  // account.
  auto it = errors_.find(account_id);
  if (it != errors_.end() && it->second.last_auth_error.IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << it->second.last_auth_error.state();
    // |ProfileOAuth2TokenService| will manage the lifetime of this pointer.
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer, it->second.last_auth_error);
  }
  // Or when we need to backoff.
  if (backoff_entry_.ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << backoff_error_.state();
    // |ProfileOAuth2TokenService| will manage the lifetime of this pointer.
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer, backoff_error_);
  }

  return account_manager_->CreateAccessTokenFetcher(
      chromeos::AccountManager::AccountKey{
          account_tracker_service_->GetAccountInfo(account_id).gaia,
          chromeos::account_manager::AccountType::
              ACCOUNT_TYPE_GAIA} /* account_key */,
      url_loader_factory, consumer);
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

void ProfileOAuth2TokenServiceDelegateChromeOS::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  backoff_entry_.InformOfRequest(!error.IsTransientError());
  ValidateAccountId(account_id);
  if (error.IsTransientError()) {
    backoff_error_ = error;
    return;
  }

  auto it = errors_.find(account_id);
  if (it != errors_.end()) {
    if (error == it->second.last_auth_error)
      return;
    // Update the existing error.
    if (error.state() == GoogleServiceAuthError::NONE)
      errors_.erase(it);
    else
      it->second.last_auth_error = error;
    FireAuthErrorChanged(account_id, error);
  } else if (error.state() != GoogleServiceAuthError::NONE) {
    // Add a new error.
    errors_.emplace(account_id, AccountErrorStatus{error});
    FireAuthErrorChanged(account_id, error);
  }
}

GoogleServiceAuthError ProfileOAuth2TokenServiceDelegateChromeOS::GetAuthError(
    const CoreAccountId& account_id) const {
  auto it = errors_.find(account_id);
  if (it != errors_.end()) {
    return it->second.last_auth_error;
  }

  return GoogleServiceAuthError::AuthErrorNone();
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

void ProfileOAuth2TokenServiceDelegateChromeOS::LoadCredentials(
    const CoreAccountId& primary_account_id) {
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

  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
  account_manager_->GetAccounts(
      base::BindOnce(&ProfileOAuth2TokenServiceDelegateChromeOS::OnGetAccounts,
                     weak_factory_.GetWeakPtr()));
}

void ProfileOAuth2TokenServiceDelegateChromeOS::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  // This API could have been called for upserting the Device/Primary
  // |account_id| or a Secondary |account_id|.

  // Account insertion:
  // Device Account insertion on Chrome OS happens as a 2 step process:
  // 1. The account is inserted into PrimaryAccountManager /
  // AccountTrackerService, via IdentityManager, with a valid Gaia id and email
  // but an invalid refresh token.
  // 2. This API is called to update the aforementioned account with a valid
  // refresh token.
  // Secondary Account insertion on Chrome OS happens atomically in
  // |InlineLoginHandlerChromeOS::<anon>::SigninHelper::OnClientOAuthSuccess|.
  // In both of the aforementioned cases, we can be sure that when this API is
  // called, |account_id| is guaranteed to be present in
  // |AccountTrackerService|. This guarantee is important because
  // |ProfileOAuth2TokenServiceDelegateChromeOS| relies on
  // |AccountTrackerService| to convert |account_id| to an email id.

  // Account update:
  // If an account is being updated, it must be present in
  // |AccountTrackerService|.

  // Hence for all cases (insertion and updates for Device and Secondary
  // Accounts) we can be sure that |account_id| is present in
  // |AccountTrackerService|.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      load_credentials_state());
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  ValidateAccountId(account_id);

  const AccountInfo& account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  LOG_IF(FATAL, account_info.gaia.empty())
      << "account_id must be present in AccountTrackerService before "
         "UpdateCredentials is called";

  // Will result in chromeos::AccountManager calling
  // |ProfileOAuth2TokenServiceDelegateChromeOS::OnTokenUpserted|.
  account_manager_->UpsertAccount(
      chromeos::AccountManager::AccountKey{
          account_info.gaia, chromeos::account_manager::AccountType::
                                 ACCOUNT_TYPE_GAIA} /* account_key */,
      account_info.email /* email */, refresh_token);
}

scoped_refptr<network::SharedURLLoaderFactory>
ProfileOAuth2TokenServiceDelegateChromeOS::GetURLLoaderFactory() const {
  if (!is_regular_profile_) {
    // Signin and Lock Screen profiles (non-|is_regular_profile_|s) have weird
    // expectations around token loading. They do not have an account associated
    // with them but expect calls like |LoadCredentials| and
    // |GetURLLoaderFactory| to successfully complete.
    // We *can* return a |nullptr| here because the return value of
    // |GetURLLoaderFactory| is never used by Signin and Lock Screen profiles.
    // They get a hard-coded |GoogleServiceAuthError::USER_NOT_SIGNED_UP| error
    // returned to them by access token fetchers.
    // We *must* return a |nullptr| here because otherwise |AccountManager|
    // DCHECKs as it has not been initialized for non-|is_regular_profile_| and
    // crashes for this weird case (Non-regular profiles expecting to act on
    // accounts).
    // See https://crbug.com/996615 for details.
    return nullptr;
  }
  return account_manager_->GetUrlLoaderFactory();
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnGetAccounts(
    const std::vector<chromeos::AccountManager::Account>& accounts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This callback should only be triggered during |LoadCredentials|, which
  // implies that |load_credentials_state())| should in
  // |LOAD_CREDENTIALS_IN_PROGRESS| state.
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
      OnTokenUpserted(account);
    }
  }
  FireRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnTokenUpserted(
    const chromeos::AccountManager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  account_keys_.insert(account.key);

  if (account.key.account_type !=
      chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return;
  }

  // All Gaia accounts in Chrome OS Account Manager must have an email
  // associated with them (https://crbug.com/933307).
  DCHECK(!account.raw_email.empty());
  std::string account_id = account_tracker_service_->SeedAccountInfo(
      account.key.id /* gaia_id */, account.raw_email);
  DCHECK(!account_id.empty());

  // Clear any previously cached errors for |account_id|.
  // We cannot directly use |UpdateAuthError| because it does not invoke
  // |FireAuthErrorChanged| if |account_id|'s error state was already
  // |GoogleServiceAuthError::State::NONE|, but |FireAuthErrorChanged| must be
  // invoked here, regardless. See the comment above |FireAuthErrorChanged| few
  // lines down.
  errors_.erase(account_id);
  GoogleServiceAuthError error(GoogleServiceAuthError::AuthErrorNone());

  // However, if we know that |account_key| has a dummy token, store a
  // persistent error against it, so that we can pre-emptively reject access
  // token requests for it.
  if (account_manager_->HasDummyGaiaToken(account.key)) {
    error = GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
            CREDENTIALS_REJECTED_BY_CLIENT);
    errors_.emplace(account_id, AccountErrorStatus{error});
  }

  ScopedBatchChange batch(this);
  FireRefreshTokenAvailable(account_id);
  // See |ProfileOAuth2TokenServiceObserver::OnAuthErrorChanged|.
  // |OnAuthErrorChanged| must be always called after
  // |OnRefreshTokenAvailable|, when refresh token is updated.
  FireAuthErrorChanged(account_id, error);
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnAccountRemoved(
    const chromeos::AccountManager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      load_credentials_state());

  auto it = account_keys_.find(account.key);
  if (it == account_keys_.end()) {
    return;
  }
  account_keys_.erase(it);

  if (account.key.account_type !=
      chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return;
  }
  std::string account_id =
      account_tracker_service_
          ->FindAccountInfoByGaiaId(account.key.id /* gaia_id */)
          .account_id;
  DCHECK(!account_id.empty());

  ScopedBatchChange batch(this);

  // ProfileOAuth2TokenService will clear its cache for |account_id| when this
  // is called. See |ProfileOAuth2TokenService::OnRefreshTokenRevoked|.
  FireRefreshTokenRevoked(account_id);
}

void ProfileOAuth2TokenServiceDelegateChromeOS::RevokeCredentials(
    const CoreAccountId& account_id) {
  // Signing out of Chrome is not possible on Chrome OS.
  NOTREACHED();
}

void ProfileOAuth2TokenServiceDelegateChromeOS::RevokeAllCredentials() {
  // Signing out of Chrome is not possible on Chrome OS.
  NOTREACHED();
}

const net::BackoffEntry*
ProfileOAuth2TokenServiceDelegateChromeOS::BackoffEntry() const {
  return &backoff_entry_;
}

void ProfileOAuth2TokenServiceDelegateChromeOS::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  backoff_entry_.Reset();
}

}  // namespace signin
