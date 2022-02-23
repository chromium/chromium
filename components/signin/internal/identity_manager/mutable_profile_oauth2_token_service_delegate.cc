// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/webdata/common/web_data_service_base.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const char kAccountIdPrefix[] = "AccountId-";
const size_t kAccountIdPrefixLength = 10;

// Enum for the Signin.LoadTokenFromDB histogram.
// Do not modify, or add or delete other than directly before
// NUM_LOAD_TOKEN_FROM_DB_STATUS.
enum class LoadTokenFromDBStatus {
  // Token was loaded.
  TOKEN_LOADED = 0,
  // Token was revoked as part of Dice migration.
  TOKEN_REVOKED_DICE_MIGRATION = 1,
  // Token was revoked because it is a secondary account and account consistency
  // is disabled.
  TOKEN_REVOKED_SECONDARY_ACCOUNT = 2,
  // Token was revoked on load due to cookie settings.
  TOKEN_REVOKED_ON_LOAD = 3,

  NUM_LOAD_TOKEN_FROM_DB_STATUS
};

std::string ApplyAccountIdPrefix(const std::string& account_id) {
  return kAccountIdPrefix + account_id;
}

bool IsLegacyRefreshTokenId(const std::string& service_id) {
  return service_id == GaiaConstants::kGaiaOAuth2LoginRefreshToken;
}

bool IsLegacyServiceId(const std::string& account_id) {
  return account_id.compare(0u, kAccountIdPrefixLength, kAccountIdPrefix) != 0;
}

CoreAccountId RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  return CoreAccountId::FromString(
      prefixed_account_id.substr(kAccountIdPrefixLength));
}

signin::LoadCredentialsState LoadCredentialsStateFromTokenResult(
    TokenServiceTable::Result token_result) {
  switch (token_result) {
    case TokenServiceTable::TOKEN_DB_RESULT_SQL_INVALID_STATEMENT:
    case TokenServiceTable::TOKEN_DB_RESULT_BAD_ENTRY:
      return signin::LoadCredentialsState::
          LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS;
    case TokenServiceTable::TOKEN_DB_RESULT_DECRYPT_ERROR:
      return signin::LoadCredentialsState::
          LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS;
    case TokenServiceTable::TOKEN_DB_RESULT_SUCCESS:
      return signin::LoadCredentialsState::
          LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS;
  }
  NOTREACHED();
  return signin::LoadCredentialsState::
      LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS;
}

// Returns whether the token service should be migrated to Dice.
// Migration can happen if the following conditions are met:
// - Token service Dice migration is not already done,
// - Account consistency is DiceMigration or greater.
// TODO(droger): Remove this code once Dice is fully enabled.
bool ShouldMigrateToDice(signin::AccountConsistencyMethod account_consistency,
                         PrefService* prefs) {
  return account_consistency == signin::AccountConsistencyMethod::kDice &&
         !prefs->GetBoolean(prefs::kTokenServiceDiceCompatible);
}

}  // namespace

// This class sends a request to GAIA to revoke the given refresh token from
// the server.  This is a best effort attempt only.  This class deletes itself
// when done successfully or otherwise.
class MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken
    : public GaiaAuthConsumer {
 public:
  RevokeServerRefreshToken(
      MutableProfileOAuth2TokenServiceDelegate* token_service_delegate,
      SigninClient* client,
      const std::string& refresh_token,
      int attempt);

  RevokeServerRefreshToken(const RevokeServerRefreshToken&) = delete;
  RevokeServerRefreshToken& operator=(const RevokeServerRefreshToken&) = delete;

  ~RevokeServerRefreshToken() override;

 private:
  // Starts the network request.
  void Start();
  // Returns true if the request should be retried.
  bool ShouldRetry(GaiaAuthConsumer::TokenRevocationStatus status);
  // GaiaAuthConsumer overrides:
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

  raw_ptr<MutableProfileOAuth2TokenServiceDelegate> token_service_delegate_;
  GaiaAuthFetcher fetcher_;
  std::string refresh_token_;
  int attempt_;
  base::WeakPtrFactory<RevokeServerRefreshToken> weak_ptr_factory_{this};
};

MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    RevokeServerRefreshToken(
        MutableProfileOAuth2TokenServiceDelegate* token_service_delegate,
        SigninClient* client,
        const std::string& refresh_token,
        int attempt)
    : token_service_delegate_(token_service_delegate),
      fetcher_(this,
               gaia::GaiaSource::kChrome,
               token_service_delegate_->GetURLLoaderFactory()),
      refresh_token_(refresh_token),
      attempt_(attempt) {
  client->DelayNetworkCall(
      base::BindRepeating(&MutableProfileOAuth2TokenServiceDelegate::
                              RevokeServerRefreshToken::Start,
                          weak_ptr_factory_.GetWeakPtr()));
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    Start() {
  fetcher_.StartRevokeOAuth2Token(refresh_token_);
}

MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    ~RevokeServerRefreshToken() {}

bool MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    ShouldRetry(GaiaAuthConsumer::TokenRevocationStatus status) {
  // Token revocation can be retried up to 3 times.
  if (attempt_ >= 2)
    return false;

  switch (status) {
    case GaiaAuthConsumer::TokenRevocationStatus::kServerError:
    case GaiaAuthConsumer::TokenRevocationStatus::kConnectionFailed:
    case GaiaAuthConsumer::TokenRevocationStatus::kConnectionTimeout:
    case GaiaAuthConsumer::TokenRevocationStatus::kConnectionCanceled:
      return true;
    case GaiaAuthConsumer::TokenRevocationStatus::kSuccess:
    case GaiaAuthConsumer::TokenRevocationStatus::kInvalidToken:
    case GaiaAuthConsumer::TokenRevocationStatus::kInvalidRequest:
    case GaiaAuthConsumer::TokenRevocationStatus::kUnknownError:
      return false;
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    OnOAuth2RevokeTokenCompleted(
        GaiaAuthConsumer::TokenRevocationStatus status) {
  if (ShouldRetry(status)) {
    token_service_delegate_->server_revokes_.push_back(
        std::make_unique<RevokeServerRefreshToken>(
            token_service_delegate_, token_service_delegate_->client_,
            refresh_token_, attempt_ + 1));
  }
  // |this| pointer will be deleted when removed from the vector, so don't
  // access any members after call to erase().
  token_service_delegate_->server_revokes_.erase(std::find_if(
      token_service_delegate_->server_revokes_.begin(),
      token_service_delegate_->server_revokes_.end(),
      [this](const std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate::
                                       RevokeServerRefreshToken>& item) {
        return item.get() == this;
      }));
}

MutableProfileOAuth2TokenServiceDelegate::
    MutableProfileOAuth2TokenServiceDelegate(
        SigninClient* client,
        AccountTrackerService* account_tracker_service,
        network::NetworkConnectionTracker* network_connection_tracker,
        scoped_refptr<TokenWebData> token_web_data,
        signin::AccountConsistencyMethod account_consistency,
        bool revoke_all_tokens_on_load,
        FixRequestErrorCallback fix_request_error_callback)
    : web_data_service_request_(0),
      backoff_entry_(&backoff_policy_),
      backoff_error_(GoogleServiceAuthError::NONE),
      client_(client),
      account_tracker_service_(account_tracker_service),
      network_connection_tracker_(network_connection_tracker),
      token_web_data_(token_web_data),
      account_consistency_(account_consistency),
      revoke_all_tokens_on_load_(revoke_all_tokens_on_load),
      fix_request_error_callback_(fix_request_error_callback) {
  VLOG(1) << "MutablePO2TS::MutablePO2TS";
  DCHECK(client);
  DCHECK(account_tracker_service_);
  DCHECK(network_connection_tracker_);
  DCHECK_NE(signin::AccountConsistencyMethod::kMirror, account_consistency_);
  // It's okay to fill the backoff policy after being used in construction.
  backoff_policy_.num_errors_to_ignore = 0;
  backoff_policy_.initial_delay_ms = 1000;
  backoff_policy_.multiply_factor = 2.0;
  backoff_policy_.jitter_factor = 0.2;
  backoff_policy_.maximum_backoff_ms = 15 * 60 * 1000;
  backoff_policy_.entry_lifetime_ms = -1;
  backoff_policy_.always_use_initial_delay = false;
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

MutableProfileOAuth2TokenServiceDelegate::
    ~MutableProfileOAuth2TokenServiceDelegate() {
  VLOG(1) << "MutablePO2TS::~MutablePO2TS";
  DCHECK(server_revokes_.empty());
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

// static
void MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kTokenServiceDiceCompatible, false);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
MutableProfileOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  ValidateAccountId(account_id);
  // check whether the account has persistent error.
  if (refresh_tokens_[account_id].last_auth_error.IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << refresh_tokens_[account_id].last_auth_error.state();
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer, refresh_tokens_[account_id].last_auth_error);
  }
  if (backoff_entry_.ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << backoff_error_.state();
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer, backoff_error_);
  }
  std::string refresh_token = GetRefreshToken(account_id);
  DCHECK(!refresh_token.empty());
  return GaiaAccessTokenFetcher::
      CreateExchangeRefreshTokenForAccessTokenInstance(
          consumer, url_loader_factory, refresh_token);
}

GoogleServiceAuthError MutableProfileOAuth2TokenServiceDelegate::GetAuthError(
    const CoreAccountId& account_id) const {
  auto it = refresh_tokens_.find(account_id);
  return (it == refresh_tokens_.end()) ? GoogleServiceAuthError::AuthErrorNone()
                                       : it->second.last_auth_error;
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "MutablePO2TS::UpdateAuthError. Error: " << error.state()
          << " account_id=" << account_id;
  backoff_entry_.InformOfRequest(!error.IsTransientError());
  ValidateAccountId(account_id);

  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.  We do keep it around though
  // to report for future requests being denied for "backoff" reasons.
  if (error.IsTransientError()) {
    backoff_error_ = error;
    return;
  }

  if (refresh_tokens_.count(account_id) == 0) {
    // This could happen if the preferences have been corrupted (see
    // http://crbug.com/321370). In a Debug build that would be a bug, but in a
    // Release build we want to deal with it gracefully.
    NOTREACHED();
    return;
  }

  AccountStatus* status = &refresh_tokens_[account_id];
  if (error != status->last_auth_error) {
    status->last_auth_error = error;
    FireAuthErrorChanged(account_id, error);
  }
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetTokenForMultilogin(
    const CoreAccountId& account_id) const {
  auto iter = refresh_tokens_.find(account_id);
  if (iter == refresh_tokens_.end() ||
      iter->second.last_auth_error != GoogleServiceAuthError::AuthErrorNone()) {
    return std::string();
  }
  const std::string& refresh_token = iter->second.refresh_token;
  DCHECK(!refresh_token.empty());
  return refresh_token;
}

bool MutableProfileOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  VLOG(1) << "MutablePO2TS::RefreshTokenIsAvailable";
  return !GetRefreshToken(account_id).empty();
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetRefreshToken(
    const CoreAccountId& account_id) const {
  auto iter = refresh_tokens_.find(account_id);
  if (iter != refresh_tokens_.end()) {
    const std::string refresh_token = iter->second.refresh_token;
    DCHECK(!refresh_token.empty());
    return refresh_token;
  }
  return std::string();
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetRefreshTokenForTest(
    const CoreAccountId& account_id) const {
  return GetRefreshToken(account_id);
}

std::vector<CoreAccountId>
MutableProfileOAuth2TokenServiceDelegate::GetAccounts() const {
  std::vector<CoreAccountId> account_ids;
  for (auto& token : refresh_tokens_) {
    account_ids.push_back(token.first);
  }
  return account_ids;
}

scoped_refptr<network::SharedURLLoaderFactory>
MutableProfileOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return client_->GetURLLoaderFactory();
}

void MutableProfileOAuth2TokenServiceDelegate::InvalidateTokenForMultilogin(
    const CoreAccountId& failed_account) {
  UpdateAuthError(
      failed_account,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void MutableProfileOAuth2TokenServiceDelegate::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  if (load_credentials_state() ==
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS) {
    VLOG(1) << "Load credentials operation already in progress";
    return;
  }

  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS);

  if (!primary_account_id.empty())
    ValidateAccountId(primary_account_id);
  DCHECK(loading_primary_account_id_.empty());
  DCHECK_EQ(0, web_data_service_request_);

  refresh_tokens_.clear();

  if (!token_web_data_) {
    // This case only exists in unit tests that do not care about loading
    // credentials.
    set_load_credentials_state(
        signin::LoadCredentialsState::
            LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS);
    MaybeDeletePreDiceTokens();
    FinishLoadingCredentials();
    return;
  }

  // If |account_id| is an email address, then canonicalize it. This is needed
  // to support legacy account IDs, and will not be needed after switching to
  // gaia IDs.
  if (primary_account_id.ToString().find('@') != std::string::npos) {
    loading_primary_account_id_ = CoreAccountId::FromEmail(
        gaia::CanonicalizeEmail(primary_account_id.ToString()));
  } else {
    loading_primary_account_id_ = primary_account_id;
  }

  web_data_service_request_ = token_web_data_->GetAllTokens(this);
}

void MutableProfileOAuth2TokenServiceDelegate::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  VLOG(1) << "MutablePO2TS::OnWebDataServiceRequestDone. Result type: "
          << (result.get() == nullptr ? -1
                                      : static_cast<int>(result->GetType()));

  DCHECK_EQ(web_data_service_request_, handle);
  web_data_service_request_ = 0;

  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<TokenResult>* token_result =
        static_cast<const WDResult<TokenResult>*>(result.get());
    LoadAllCredentialsIntoMemory(token_result->GetValue().tokens);
    set_load_credentials_state(LoadCredentialsStateFromTokenResult(
        token_result->GetValue().db_result));
  } else {
    set_load_credentials_state(
        signin::LoadCredentialsState::
            LOAD_CREDENTIALS_FINISHED_WITH_DB_CANNOT_BE_OPENED);
    MaybeDeletePreDiceTokens();
  }

  // Make sure that we have an entry for |loading_primary_account_id_| in the
  // map.  The entry could be missing if there is a corruption in the token DB
  // while this profile is connected to an account.
  if (!loading_primary_account_id_.empty() &&
      refresh_tokens_.count(loading_primary_account_id_) == 0) {
    if (load_credentials_state() ==
        signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
      set_load_credentials_state(
          signin::LoadCredentialsState::
              LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT);
    }
    AddAccountStatus(loading_primary_account_id_,
                     GaiaConstants::kInvalidRefreshToken,
                     GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_MISSING));
    FireRefreshTokenAvailable(loading_primary_account_id_);
  }

#ifndef NDEBUG
  for (auto& token : refresh_tokens_) {
    DCHECK(RefreshTokenIsAvailable(token.first))
        << "Missing token for " << token.first;
  }
#endif

  loading_primary_account_id_ = CoreAccountId();
  FinishLoadingCredentials();
}

void MutableProfileOAuth2TokenServiceDelegate::LoadAllCredentialsIntoMemory(
    const std::map<std::string, std::string>& db_tokens) {
  std::string old_login_token;
  bool migrate_to_dice =
      ShouldMigrateToDice(account_consistency_, client_->GetPrefs());

  {
    ScopedBatchChange batch(this);

    VLOG(1) << "MutablePO2TS::LoadAllCredentialsIntoMemory; "
            << db_tokens.size() << " Credential(s).";
    AccountTrackerService::AccountIdMigrationState migration_state =
        account_tracker_service_->GetMigrationState();
    for (auto iter = db_tokens.begin(); iter != db_tokens.end(); ++iter) {
      std::string prefixed_account_id = iter->first;
      std::string refresh_token = iter->second;

      if (IsLegacyRefreshTokenId(prefixed_account_id) && !refresh_token.empty())
        old_login_token = refresh_token;

      if (IsLegacyServiceId(prefixed_account_id)) {
        if (token_web_data_) {
          VLOG(1) << "MutablePO2TS remove legacy refresh token for account id "
                  << prefixed_account_id;
          token_web_data_->RemoveTokenForService(prefixed_account_id);
        }
      } else {
        DCHECK(!refresh_token.empty());
        CoreAccountId account_id = RemoveAccountIdPrefix(prefixed_account_id);

        switch (migration_state) {
          case AccountTrackerService::MIGRATION_IN_PROGRESS: {
            // Migrate to gaia-ids.
            AccountInfo account_info =
                account_tracker_service_->FindAccountInfoByEmail(
                    account_id.ToString());
            // |account_info| can be empty if |account_id| was already migrated.
            // This could happen if the chrome was closed in the middle of the
            // account id migration.
            if (!account_info.IsEmpty()) {
              ClearPersistedCredentials(account_id);
              account_id = account_info.account_id;
              PersistCredentials(account_id, refresh_token);
            }

            // Skip duplicate accounts, this could happen if migration was
            // crashed in the middle.
            if (refresh_tokens_.count(account_id) != 0)
              continue;
            break;
          }
          case AccountTrackerService::MIGRATION_NOT_STARTED:
            // If the account_id is an email address, then canonicalize it. This
            // is to support legacy account_ids, and will not be needed after
            // switching to gaia-ids.
            if (account_id.ToString().find('@') != std::string::npos) {
              // If the canonical account id is not the same as the loaded
              // account id, make sure not to overwrite a refresh token from
              // a canonical version.  If no canonical version was loaded, then
              // re-persist this refresh token with the canonical account id.
              CoreAccountId canon_account_id = CoreAccountId::FromEmail(
                  gaia::CanonicalizeEmail(account_id.ToString()));
              if (canon_account_id != account_id) {
                ClearPersistedCredentials(account_id);
                if (db_tokens.count(
                        ApplyAccountIdPrefix(canon_account_id.ToString())) == 0)
                  PersistCredentials(canon_account_id, refresh_token);
              }
              account_id = canon_account_id;
            }
            break;
          case AccountTrackerService::MIGRATION_DONE:
            DCHECK_EQ(std::string::npos, account_id.ToString().find('@'));
            break;
          case AccountTrackerService::NUM_MIGRATION_STATES:
            NOTREACHED();
            break;
        }

        // Only load secondary accounts when account consistency is enabled.
        bool load_account =
            account_id == loading_primary_account_id_ ||
            account_consistency_ == signin::AccountConsistencyMethod::kDice;
        LoadTokenFromDBStatus load_token_status =
            load_account
                ? LoadTokenFromDBStatus::TOKEN_LOADED
                : LoadTokenFromDBStatus::TOKEN_REVOKED_SECONDARY_ACCOUNT;

        if (migrate_to_dice) {
          // Revoke old hosted domain accounts as part of Dice migration.
          AccountInfo account_info =
              account_tracker_service_->GetAccountInfo(account_id);
          bool is_hosted_domain = false;
          if (account_info.hosted_domain.empty()) {
            // The AccountInfo is incomplete. Use a conservative approximation.
            is_hosted_domain =
                !client_->IsNonEnterpriseUser(account_info.email);
          } else {
            is_hosted_domain =
                (account_info.hosted_domain != kNoHostedDomainFound);
          }
          if (is_hosted_domain) {
            load_account = false;
            load_token_status =
                LoadTokenFromDBStatus::TOKEN_REVOKED_DICE_MIGRATION;
          }
        }

        if (load_account && revoke_all_tokens_on_load_) {
          if (account_id == loading_primary_account_id_) {
            RevokeCredentialsOnServer(refresh_token);
            refresh_token = GaiaConstants::kInvalidRefreshToken;
            PersistCredentials(account_id, refresh_token);
          } else {
            load_account = false;
          }
          load_token_status = LoadTokenFromDBStatus::TOKEN_REVOKED_ON_LOAD;
        }

        UMA_HISTOGRAM_ENUMERATION(
            "Signin.LoadTokenFromDB", load_token_status,
            LoadTokenFromDBStatus::NUM_LOAD_TOKEN_FROM_DB_STATUS);

        if (load_account) {
          UpdateCredentialsInMemory(account_id, refresh_token);
          FireRefreshTokenAvailable(account_id);
        } else {
          RevokeCredentialsOnServer(refresh_token);
          ClearPersistedCredentials(account_id);
          FireRefreshTokenRevoked(account_id);
        }
      }
    }

    if (!old_login_token.empty()) {
      DCHECK(!loading_primary_account_id_.empty());
      if (refresh_tokens_.count(loading_primary_account_id_) == 0)
        UpdateCredentials(loading_primary_account_id_, old_login_token);
    }
  }

  if (migrate_to_dice)
    client_->GetPrefs()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  ValidateAccountId(account_id);
  const std::string& existing_token = GetRefreshToken(account_id);
  if (existing_token != refresh_token) {
    ScopedBatchChange batch(this);
    UpdateCredentialsInMemory(account_id, refresh_token);
    PersistCredentials(account_id, refresh_token);
    FireRefreshTokenAvailable(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateCredentialsInMemory(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  bool is_refresh_token_invalidated =
      refresh_token == GaiaConstants::kInvalidRefreshToken;
  GoogleServiceAuthError error =
      is_refresh_token_invalidated
          ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT)
          : GoogleServiceAuthError::AuthErrorNone();

  bool refresh_token_present = refresh_tokens_.count(account_id) > 0;
  // If token present, and different from the new one, cancel its requests,
  // and clear the entries in cache related to that account.
  if (refresh_token_present) {
    DCHECK_NE(refresh_token, refresh_tokens_[account_id].refresh_token);
    VLOG(1) << "MutablePO2TS::UpdateCredentials; Refresh Token was present. "
            << "account_id=" << account_id;

    // The old refresh token must be revoked on the server only when it is
    // invalidated.
    //
    // The refresh token is updated to a new valid one in case of reauth.
    // In the reauth case the old and the new refresh tokens have the same
    // device ID. When revoking a refresh token on the server, Gaia revokes
    // all the refresh tokens that have the same device ID.
    // Therefore, the old refresh token must not be revoked on the server
    // when it is updated to a new valid one (otherwise the new refresh token
    // would also be invalidated server-side).
    // See http://crbug.com/865189 for more information about this regression.
    if (is_refresh_token_invalidated)
      RevokeCredentialsOnServer(refresh_tokens_[account_id].refresh_token);

    refresh_tokens_[account_id].refresh_token = refresh_token;
    UpdateAuthError(account_id, error);
  } else {
    VLOG(1) << "MutablePO2TS::UpdateCredentials; Refresh Token was absent. "
            << "account_id=" << account_id;
    AddAccountStatus(account_id, refresh_token, error);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::PersistCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());
  if (token_web_data_) {
    VLOG(1) << "MutablePO2TS::PersistCredentials for account_id=" << account_id;
    token_web_data_->SetTokenForService(
        ApplyAccountIdPrefix(account_id.ToString()), refresh_token);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeAllCredentials() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "MutablePO2TS::RevokeAllCredentials";

  ScopedBatchChange batch(this);
  if (load_credentials_state() ==
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS) {
    VLOG(1) << "MutablePO2TS::RevokeAllCredentials before tokens are loaded.";
    // If |RevokeAllCredentials| is called while credentials are being loaded,
    // then the tokens should be revoked on load.
    revoke_all_tokens_on_load_ = true;
    loading_primary_account_id_ = CoreAccountId();
  }

  // Make a temporary copy of the account ids.
  std::vector<CoreAccountId> accounts;
  for (const auto& token : refresh_tokens_)
    accounts.push_back(token.first);
  for (const auto& account : accounts)
    RevokeCredentials(account);

  DCHECK_EQ(0u, refresh_tokens_.size());

  // Make sure all tokens are removed from storage.
  if (token_web_data_)
    token_web_data_->RemoveAllTokens();
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentials(
    const CoreAccountId& account_id) {
  RevokeCredentialsImpl(account_id, /*revoke_on_server=*/true);
}

void MutableProfileOAuth2TokenServiceDelegate::ClearPersistedCredentials(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  if (token_web_data_) {
    VLOG(1) << "MutablePO2TS::ClearPersistedCredentials for account_id="
            << account_id;
    token_web_data_->RemoveTokenForService(
        ApplyAccountIdPrefix(account_id.ToString()));
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentialsOnServer(
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());

  if (refresh_token == GaiaConstants::kInvalidRefreshToken)
    return;

  // Keep track or all server revoke requests.  This way they can be deleted
  // before the token service is shutdown and won't outlive the profile.
  server_revokes_.push_back(std::make_unique<RevokeServerRefreshToken>(
      this, client_, refresh_token, 0));
}

void MutableProfileOAuth2TokenServiceDelegate::CancelWebTokenFetch() {
  if (web_data_service_request_ != 0) {
    DCHECK(token_web_data_);
    token_web_data_->CancelRequest(web_data_service_request_);
    web_data_service_request_ = 0;
  }
}

void MutableProfileOAuth2TokenServiceDelegate::ExtractCredentials(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  static_cast<ProfileOAuth2TokenService*>(to_service)
      ->UpdateCredentials(account_id, GetRefreshToken(account_id),
                          signin_metrics::SourceForRefreshTokenOperation::
                              kTokenService_ExtractCredentials);
  RevokeCredentialsImpl(account_id, /*revoke_on_server=*/false);
}

void MutableProfileOAuth2TokenServiceDelegate::Shutdown() {
  VLOG(1) << "MutablePO2TS::Shutdown";
  server_revokes_.clear();
  CancelWebTokenFetch();
  refresh_tokens_.clear();
  ProfileOAuth2TokenServiceDelegate::Shutdown();
}

void MutableProfileOAuth2TokenServiceDelegate::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  // If our network has changed, reset the backoff timer so that errors caused
  // by a previous lack of network connectivity don't prevent new requests.
  backoff_entry_.Reset();
}

const net::BackoffEntry*
MutableProfileOAuth2TokenServiceDelegate::BackoffEntry() const {
  return &backoff_entry_;
}

bool MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorIfPossible() {
  return !fix_request_error_callback_.is_null()
             ? fix_request_error_callback_.Run()
             : false;
}

void MutableProfileOAuth2TokenServiceDelegate::AddAccountStatus(
    const CoreAccountId& account_id,
    const std::string& refresh_token,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(0u, refresh_tokens_.count(account_id));
  refresh_tokens_[account_id] = AccountStatus{refresh_token, error};
  FireAuthErrorChanged(account_id, error);
}

void MutableProfileOAuth2TokenServiceDelegate::FinishLoadingCredentials() {
  if (account_consistency_ == signin::AccountConsistencyMethod::kDice)
    DCHECK(client_->GetPrefs()->GetBoolean(prefs::kTokenServiceDiceCompatible));
  FireRefreshTokensLoaded();
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentialsImpl(
    const CoreAccountId& account_id,
    bool revoke_on_server) {
  ValidateAccountId(account_id);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (refresh_tokens_.count(account_id) > 0) {
    VLOG(1) << "MutablePO2TS::RevokeCredentials for account_id=" << account_id;
    ScopedBatchChange batch(this);
    const std::string& token = refresh_tokens_[account_id].refresh_token;
    if (revoke_on_server)
      RevokeCredentialsOnServer(token);
    refresh_tokens_.erase(account_id);
    ClearPersistedCredentials(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::MaybeDeletePreDiceTokens() {
  DCHECK(load_credentials_state() ==
             signin::LoadCredentialsState::
                 LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS ||
         load_credentials_state() ==
             signin::LoadCredentialsState::
                 LOAD_CREDENTIALS_FINISHED_WITH_DB_CANNOT_BE_OPENED);

  if (account_consistency_ == signin::AccountConsistencyMethod::kDice &&
      !client_->GetPrefs()->GetBoolean(prefs::kTokenServiceDiceCompatible)) {
    RevokeAllCredentials();
    client_->GetPrefs()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);
  }
}
