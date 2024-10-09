// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/webdata/common/web_data_service_base.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/signin/internal/identity_manager/token_binding_helper.h"
#include "components/signin/internal/identity_manager/token_binding_oauth2_access_token_fetcher.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_mint_access_token_fetcher_adapter.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace {

const char kAccountIdPrefix[] = "AccountId-";

// Enum for the Signin.LoadTokenFromDB histogram.
// Do not modify, or add or delete other than directly before
// NUM_LOAD_TOKEN_FROM_DB_STATUS.
enum class LoadTokenFromDBStatus {
  // Token was loaded.
  TOKEN_LOADED = 0,

  // DEPRECATED
  // Token was revoked as part of Dice migration.
  // TOKEN_REVOKED_DICE_MIGRATION = 1,

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

// Checks that |prefixed_account_id| starts with the expected prefix
// (|prefixed_account_id|) and returns the non-prefixed account id or an empty
// account id if |prefixed_account_id| is not correctly prefixed.
CoreAccountId RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  if (!base::StartsWith(prefixed_account_id, kAccountIdPrefix))
    return CoreAccountId();

  return CoreAccountId::FromString(
      prefixed_account_id.substr(/*pos=*/strlen(kAccountIdPrefix)));
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
  NOTREACHED_IN_MIGRATION();
  return signin::LoadCredentialsState::
      LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS;
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(BoundTokenPrevalence)
enum class BoundTokenPrevalence {
  kEmpty = 0,
  kZeroTokensBound = 1,
  kSomeTokensBoundSomeUnbound = 2,
  kAllTokensBound = 3,
  kMaxValue = kAllTokensBound
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:TokenBindingBoundTokenPrevalence)

void RecordTokenBindingHistogramsOnCredentialsLoaded(
    TokenBindingHelper* token_binding_helper,
    size_t token_count) {
  size_t bound_token_count =
      token_binding_helper ? token_binding_helper->GetBoundTokenCount() : 0;
  CHECK_LE(bound_token_count, token_count);

  BoundTokenPrevalence prevalence = [token_count, bound_token_count] {
    if (token_count == 0) {
      return BoundTokenPrevalence::kEmpty;
    } else if (bound_token_count == 0) {
      return BoundTokenPrevalence::kZeroTokensBound;
    } else if (token_count == bound_token_count) {
      return BoundTokenPrevalence::kAllTokensBound;
    } else {
      return BoundTokenPrevalence::kSomeTokensBoundSomeUnbound;
    }
  }();

  base::UmaHistogramEnumeration("Signin.TokenBinding.BoundTokenPrevalence",
                                prevalence);
  if (bound_token_count > 1) {
    CHECK(token_binding_helper);
    base::UmaHistogramBoolean("Signin.TokenBinding.BoundToTheSameKey",
                              token_binding_helper->AreAllBindingKeysSame());
  }
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

// This feature controls whether or not token data is re-encrypted when OSCrypt
// indicates that it should be. This is intended as an emergency 'off-switch' in
// case any unexpected issues are encountered in the key migration.
// TODO(crbug.com/366375488): Remove once migration is proven to work reliably.
namespace features {
BASE_FEATURE(kEnableReEncryptOfTokenData,
             "EnableReEncryptOfTokenData",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

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
    ~RevokeServerRefreshToken() = default;

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
  token_service_delegate_->server_revokes_.erase(base::ranges::find(
      token_service_delegate_->server_revokes_, this,
      &std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate::
                           RevokeServerRefreshToken>::get));
}

MutableProfileOAuth2TokenServiceDelegate::
    MutableProfileOAuth2TokenServiceDelegate(
        SigninClient* client,
        AccountTrackerService* account_tracker_service,
        network::NetworkConnectionTracker* network_connection_tracker,
        scoped_refptr<TokenWebData> token_web_data,
        signin::AccountConsistencyMethod account_consistency,
        RevokeAllTokensOnLoad revoke_all_tokens_on_load,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        std::unique_ptr<TokenBindingHelper> token_binding_helper,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        FixRequestErrorCallback fix_request_error_callback)
    : ProfileOAuth2TokenServiceDelegate(/*use_backoff=*/true),
      web_data_service_request_(0),
      client_(client),
      account_tracker_service_(account_tracker_service),
      network_connection_tracker_(network_connection_tracker),
      token_web_data_(token_web_data),
      account_consistency_(account_consistency),
      revoke_all_tokens_on_load_(revoke_all_tokens_on_load),
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      token_binding_helper_(std::move(token_binding_helper)),
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      fix_request_error_callback_(fix_request_error_callback) {
  VLOG(1) << "MutablePO2TS::MutablePO2TS";
  DCHECK(client);
  DCHECK(account_tracker_service_);
  DCHECK(network_connection_tracker_);
  DCHECK_NE(signin::AccountConsistencyMethod::kMirror, account_consistency_);
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

MutableProfileOAuth2TokenServiceDelegate::
    ~MutableProfileOAuth2TokenServiceDelegate() {
  VLOG(1) << "MutablePO2TS::~MutablePO2TS";
  DCHECK(server_revokes_.empty());
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
MutableProfileOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  ValidateAccountId(account_id);
  // check whether the account has persistent error.
  GoogleServiceAuthError auth_error = GetAuthError(account_id);
  if (auth_error.IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << auth_error.state();
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(consumer,
                                                                    auth_error);
  }
  if (BackoffEntry()->ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << BackOffError().state();
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer, BackOffError());
  }
  std::string refresh_token = GetRefreshToken(account_id);
  DCHECK(!refresh_token.empty());
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (token_binding_helper_ &&
      token_binding_helper_->HasBindingKey(account_id)) {
    const std::string gaia_id =
        account_tracker_service_->GetAccountInfo(account_id).gaia;
    CHECK(!gaia_id.empty());
    // `GaiaAccessTokenFetcher` doesn't support bound refresh tokens.
    auto fetcher = std::make_unique<OAuth2MintAccessTokenFetcherAdapter>(
        consumer, url_loader_factory, gaia_id, refresh_token,
        signin::GetSigninScopedDeviceId(client_->GetPrefs()),
        std::string(version_info::GetVersionNumber()),
        std::string(
            version_info::GetChannelString(client_->GetClientChannel())));
    if (token_binding_challenge.empty()) {
      return fetcher;
    }
    // `fetcher_wrapper` makes `fetcher` wait until a binding key assertion is
    // generated before sending a network request.
    auto fetcher_wrapper =
        std::make_unique<TokenBindingOAuth2AccessTokenFetcher>(
            std::move(fetcher));
    token_binding_helper_->GenerateBindingKeyAssertion(
        account_id, token_binding_challenge,
        GURL("https://accounts.google.com/accountmanager"),
        base::BindOnce(
            &TokenBindingOAuth2AccessTokenFetcher::SetBindingKeyAssertion,
            fetcher_wrapper->GetWeakPtr()));
    return fetcher_wrapper;
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  return GaiaAccessTokenFetcher::
      CreateExchangeRefreshTokenForAccessTokenInstance(
          consumer, url_loader_factory, refresh_token);
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetTokenForMultilogin(
    const CoreAccountId& account_id) const {
  auto iter = refresh_tokens_.find(account_id);
  if (iter == refresh_tokens_.end() ||
      GetAuthError(account_id) != GoogleServiceAuthError::AuthErrorNone()) {
    return std::string();
  }
  const std::string& refresh_token = iter->second;
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
    const std::string refresh_token = iter->second;
    DCHECK(!refresh_token.empty());
    return refresh_token;
  }
  return std::string();
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
std::vector<uint8_t>
MutableProfileOAuth2TokenServiceDelegate::GetWrappedBindingKey(
    const CoreAccountId& account_id) const {
  if (!token_binding_helper_) {
    return {};
  }

  return token_binding_helper_->GetWrappedBindingKey(account_id);
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

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
  UpdateAuthError(failed_account,
                  GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                          CREDENTIALS_REJECTED_BY_SERVER));
}

void MutableProfileOAuth2TokenServiceDelegate::LoadCredentialsInternal(
    const CoreAccountId& primary_account_id,
    bool is_syncing) {
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
  ClearAuthError(std::nullopt);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (token_binding_helper_) {
    token_binding_helper_->ClearAllKeys();
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  if (!token_web_data_) {
    // This case only exists in unit tests that do not care about loading
    // credentials.
    set_load_credentials_state(
        signin::LoadCredentialsState::
            LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS);
    FinishLoadingCredentials();
    return;
  }

  loading_primary_account_id_ = primary_account_id;
  loading_is_syncing_ = is_syncing;
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
  ScopedBatchChange batch(this);

  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<TokenResult>* token_result =
        static_cast<const WDResult<TokenResult>*>(result.get());
    LoadAllCredentialsIntoMemory(token_result->GetValue().tokens,
                                 token_result->GetValue().should_reencrypt);
    set_load_credentials_state(LoadCredentialsStateFromTokenResult(
        token_result->GetValue().db_result));
  } else {
    set_load_credentials_state(
        signin::LoadCredentialsState::
            LOAD_CREDENTIALS_FINISHED_WITH_DB_CANNOT_BE_OPENED);
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
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                     /*wrapped_binding_key=*/std::vector<uint8_t>(),
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
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
  loading_is_syncing_ = false;
  FinishLoadingCredentials();
}

void MutableProfileOAuth2TokenServiceDelegate::LoadAllCredentialsIntoMemory(
    const std::map<std::string, TokenServiceTable::TokenWithBindingKey>&
        db_tokens,
    bool should_reencrypt) {
  VLOG(1) << "MutablePO2TS::LoadAllCredentialsIntoMemory; " << db_tokens.size()
          << " credential(s).";
  bool did_reencrypt = false;
  ScopedBatchChange batch(this);
  for (const auto& [prefixed_account_id, token_with_key] : db_tokens) {
    std::string refresh_token = token_with_key.token;
    std::vector<uint8_t> wrapped_binding_key =
        token_with_key.wrapped_binding_key;

    CoreAccountId account_id = RemoveAccountIdPrefix(prefixed_account_id);
    if (account_id.empty()) {
      if (token_web_data_) {
        VLOG(1) << "MutablePO2TS remove refresh token for invalid account id ["
                << prefixed_account_id << "]";
        token_web_data_->RemoveTokenForService(prefixed_account_id);
      }
      continue;
    }

    DCHECK(!account_id.IsEmail())
        << "Acount id should be a Gaia id [account_id = " << account_id << "]";
    DCHECK(!refresh_token.empty());

    // Only load secondary accounts when account consistency is enabled.
    bool load_account =
        account_id == loading_primary_account_id_ ||
        account_consistency_ == signin::AccountConsistencyMethod::kDice;
    LoadTokenFromDBStatus load_token_status =
        load_account ? LoadTokenFromDBStatus::TOKEN_LOADED
                     : LoadTokenFromDBStatus::TOKEN_REVOKED_SECONDARY_ACCOUNT;

    bool revoke_token = false;
    switch (revoke_all_tokens_on_load_) {
      case RevokeAllTokensOnLoad::kNo:
        break;
      case RevokeAllTokensOnLoad::kDeleteSiteDataOnExit:
        if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
          // With Uno, tokens are not revoked when clearing cookies if the user
          // is signed in non-syncing.
          revoke_token =
              loading_primary_account_id_.empty() || loading_is_syncing_;
        } else {
          revoke_token = true;
        }
        break;
      case RevokeAllTokensOnLoad::kExplicitRevoke:
        revoke_token = true;
        break;
    }

    if (load_account && revoke_token) {
      if (account_id == loading_primary_account_id_) {
        RevokeCredentialsOnServer(refresh_token);
        refresh_token = GaiaConstants::kInvalidRefreshToken;
        wrapped_binding_key = std::vector<uint8_t>();
        PersistCredentials(account_id, refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                           ,
                           wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        );
      } else {
        load_account = false;
      }
      load_token_status = LoadTokenFromDBStatus::TOKEN_REVOKED_ON_LOAD;
    }

    UMA_HISTOGRAM_ENUMERATION(
        "Signin.LoadTokenFromDB", load_token_status,
        LoadTokenFromDBStatus::NUM_LOAD_TOKEN_FROM_DB_STATUS);

    if (load_account) {
      if (!revoke_token && should_reencrypt) {
        if (base::FeatureList::IsEnabled(
                features::kEnableReEncryptOfTokenData)) {
          did_reencrypt = true;
          PersistCredentials(account_id, refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                             ,
                             wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
          );
        }
      }
      RecordAccountAvailabilityStartup(account_id, refresh_token);

      UpdateCredentialsInMemory(account_id, refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                                ,
                                wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      );
      FireRefreshTokenAvailable(account_id);
    } else {
      RevokeCredentialsOnServer(refresh_token);
      ClearPersistedCredentials(account_id);
      FireRefreshTokenRevoked(account_id);
    }
  }
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  RecordTokenBindingHistogramsOnCredentialsLoaded(
      token_binding_helper_.get(),
      std::ranges::count_if(refresh_tokens_, [](const auto& kv_pair) {
        return kv_pair.second != GaiaConstants::kInvalidRefreshToken;
      }));
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  base::UmaHistogramBoolean("Signin.ReencryptTokensInDb", did_reencrypt);
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateCredentialsInternal(
    const CoreAccountId& account_id,
    const std::string& refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  ValidateAccountId(account_id);
  const std::string& existing_token = GetRefreshToken(account_id);
  if (existing_token != refresh_token) {
    UpdateCredentialsInMemory(account_id, refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                              ,
                              wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    );
    PersistCredentials(account_id, refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                       ,
                       wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    );
    FireRefreshTokenAvailable(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateCredentialsInMemory(
    const CoreAccountId& account_id,
    const std::string& refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
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
    DCHECK_NE(refresh_token, refresh_tokens_[account_id]);
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
      RevokeCredentialsOnServer(refresh_tokens_[account_id]);

    refresh_tokens_[account_id] = refresh_token;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    if (token_binding_helper_) {
      token_binding_helper_->SetBindingKey(account_id, wrapped_binding_key);
    }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    UpdateAuthError(account_id, error);
  } else {
    VLOG(1) << "MutablePO2TS::UpdateCredentials; Refresh Token was absent. "
            << "account_id=" << account_id;
    AddAccountStatus(account_id, refresh_token,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                     wrapped_binding_key,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                     error);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::PersistCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());
  if (token_web_data_) {
    VLOG(1) << "MutablePO2TS::PersistCredentials for account_id=" << account_id;
    token_web_data_->SetTokenForService(
        ApplyAccountIdPrefix(account_id.ToString()), refresh_token,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        wrapped_binding_key
#else
        /*wrapped_binding_key=*/{}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    );
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeAllCredentialsInternal(
    signin_metrics::SourceForRefreshTokenOperation source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "MutablePO2TS::RevokeAllCredentials";

  ScopedBatchChange batch(this);
  if (load_credentials_state() ==
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS) {
    VLOG(1) << "MutablePO2TS::RevokeAllCredentials before tokens are loaded.";
    // If |RevokeAllCredentials| is called while credentials are being loaded,
    // then the tokens should be revoked on load.
    revoke_all_tokens_on_load_ = RevokeAllTokensOnLoad::kExplicitRevoke;
    loading_primary_account_id_ = CoreAccountId();
    loading_is_syncing_ = false;
  }

  // Make a temporary copy of the account ids.
  std::vector<CoreAccountId> accounts;
  for (const auto& token : refresh_tokens_)
    accounts.push_back(token.first);
  for (const auto& account : accounts)
    RevokeCredentials(account, source);

  DCHECK_EQ(0u, refresh_tokens_.size());

  // Make sure all tokens are removed from storage.
  if (token_web_data_)
    token_web_data_->RemoveAllTokens();
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentialsInternal(
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

void MutableProfileOAuth2TokenServiceDelegate::ExtractCredentialsInternal(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  to_service->UpdateCredentials(account_id, GetRefreshToken(account_id),
                                signin_metrics::SourceForRefreshTokenOperation::
                                    kTokenService_ExtractCredentials
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                                ,
                                GetWrappedBindingKey(account_id)
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );
  RevokeCredentialsImpl(account_id, /*revoke_on_server=*/false);
}

void MutableProfileOAuth2TokenServiceDelegate::Shutdown() {
  VLOG(1) << "MutablePO2TS::Shutdown";
  server_revokes_.clear();
  CancelWebTokenFetch();
  refresh_tokens_.clear();
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (token_binding_helper_) {
    token_binding_helper_->ClearAllKeys();
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  ProfileOAuth2TokenServiceDelegate::Shutdown();
}

void MutableProfileOAuth2TokenServiceDelegate::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  // If our network has changed, reset the backoff timer so that errors caused
  // by a previous lack of network connectivity don't prevent new requests.
  ResetBackOffEntry();
}

bool MutableProfileOAuth2TokenServiceDelegate::FixAccountErrorIfPossible() {
  return !fix_request_error_callback_.is_null()
             ? fix_request_error_callback_.Run()
             : false;
}

void MutableProfileOAuth2TokenServiceDelegate::AddAccountStatus(
    const CoreAccountId& account_id,
    const std::string& refresh_token,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    const std::vector<uint8_t>& wrapped_binding_key,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(0u, refresh_tokens_.count(account_id));
  refresh_tokens_[account_id] = refresh_token;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (token_binding_helper_) {
    token_binding_helper_->SetBindingKey(account_id, wrapped_binding_key);
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  UpdateAuthError(account_id, error, /*fire_auth_error_changed=*/false);
  FireAuthErrorChanged(account_id, error);
}

void MutableProfileOAuth2TokenServiceDelegate::FinishLoadingCredentials() {
  FireRefreshTokensLoaded();
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentialsImpl(
    const CoreAccountId& account_id,
    bool revoke_on_server) {
  ValidateAccountId(account_id);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (refresh_tokens_.count(account_id) > 0) {
    VLOG(1) << "MutablePO2TS::RevokeCredentials for account_id=" << account_id;
    if (revoke_on_server) {
      RevokeCredentialsOnServer(refresh_tokens_[account_id]);
    }
    refresh_tokens_.erase(account_id);
    ClearAuthError(account_id);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    if (token_binding_helper_) {
      token_binding_helper_->SetBindingKey(account_id, {});
    }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ClearPersistedCredentials(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RecordAccountAvailabilityStartup(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  constexpr const char kAccountAvailabilityStartupHistogramBase[] =
      "Signin.AccountInPref.StartupState.";

  bool known_account =
      !account_tracker_service_->GetAccountInfo(account_id).IsEmpty();
  bool refersh_token_valid =
      refresh_token != GaiaConstants::kInvalidRefreshToken;

  AccountStartupState startup_state = AccountStartupState::kUnknownInvalidToken;
  if (known_account && refersh_token_valid) {
    startup_state = AccountStartupState::kKnownValidToken;
  } else if (known_account) {
    startup_state = AccountStartupState::kKnownInvalidToken;
  } else if (refersh_token_valid) {
    startup_state = AccountStartupState::kUnknownValidToken;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({kAccountAvailabilityStartupHistogramBase,
                    loading_primary_account_id_ == account_id ? "Primary"
                                                              : "Secondary"}),
      startup_state);
}
