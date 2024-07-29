// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"

#include <list>
#include <memory>
#include <vector>

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

FakeProfileOAuth2TokenServiceDelegate::FakeProfileOAuth2TokenServiceDelegate()
    : ProfileOAuth2TokenServiceDelegate(/*use_backoff=*/true),
      shared_factory_(test_url_loader_factory_.GetSafeWeakWrapper()) {}

FakeProfileOAuth2TokenServiceDelegate::
    ~FakeProfileOAuth2TokenServiceDelegate() = default;

std::unique_ptr<OAuth2AccessTokenFetcher>
FakeProfileOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  auto it = refresh_tokens_.find(account_id);
  CHECK(it != refresh_tokens_.end(), base::NotFatalUntil::M130);
  return GaiaAccessTokenFetcher::
      CreateExchangeRefreshTokenForAccessTokenInstance(
          consumer, url_loader_factory, it->second);
}

bool FakeProfileOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  return !GetRefreshToken(account_id).empty();
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
std::vector<uint8_t>
FakeProfileOAuth2TokenServiceDelegate::GetWrappedBindingKey(
    const CoreAccountId& account_id) const {
  auto it = wrapped_binding_keys_.find(account_id);
  return it != wrapped_binding_keys_.end() ? it->second
                                           : std::vector<uint8_t>();
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

std::string FakeProfileOAuth2TokenServiceDelegate::GetRefreshToken(
    const CoreAccountId& account_id) const {
  auto it = refresh_tokens_.find(account_id);
  if (it != refresh_tokens_.end())
    return it->second;
  return std::string();
}

std::vector<CoreAccountId> FakeProfileOAuth2TokenServiceDelegate::GetAccounts()
    const {
  std::vector<CoreAccountId> account_ids;
  for (const auto& account_id : account_ids_)
    account_ids.push_back(account_id);
  return account_ids;
}

void FakeProfileOAuth2TokenServiceDelegate::RevokeAllCredentialsInternal(
    signin_metrics::SourceForRefreshTokenOperation source) {
  std::vector<CoreAccountId> account_ids = GetAccounts();
  if (account_ids.empty())
    return;

  // Use `ScopedBatchChange` so that `OnEndBatchOfRefreshTokenStateChanges()` is
  // fired only once, like in production.
  ScopedBatchChange batch(this);
  for (const auto& account : account_ids)
    RevokeCredentials(account, source);
}

void FakeProfileOAuth2TokenServiceDelegate::LoadCredentialsInternal(
    const CoreAccountId& primary_account_id,
    bool is_syncing) {
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  FireRefreshTokensLoaded();
}

void FakeProfileOAuth2TokenServiceDelegate::UpdateCredentialsInternal(
    const CoreAccountId& account_id,
    const std::string& refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
  IssueRefreshTokenForUser(account_id, refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                           ,
                           wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );
}

void FakeProfileOAuth2TokenServiceDelegate::IssueRefreshTokenForUser(
    const CoreAccountId& account_id,
    const std::string& token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
  if (token.empty()) {
    std::erase(account_ids_, account_id);
    refresh_tokens_.erase(account_id);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    wrapped_binding_keys_.erase(account_id);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ClearAuthError(account_id);
    FireRefreshTokenRevoked(account_id);
  } else {
    // Look for the account ID in the list, and if it is not present append it.
    if (base::ranges::find(account_ids_, account_id) == account_ids_.end()) {
      account_ids_.push_back(account_id);
    }
    refresh_tokens_[account_id] = token;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    wrapped_binding_keys_[account_id] = wrapped_binding_key;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    // If the token is a special "invalid" value, then that means the token was
    // rejected by the client and is thus not valid. So set the appropriate
    // error in that case. This logic is essentially duplicated from
    // MutableProfileOAuth2TokenServiceDelegate.
    GoogleServiceAuthError error =
        token == GaiaConstants::kInvalidRefreshToken
            ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                  GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                      CREDENTIALS_REJECTED_BY_CLIENT)
            : GoogleServiceAuthError(GoogleServiceAuthError::NONE);

    // The main difference with this call compared to the production call is
    // that it is also called for newly added accounts.
    UpdateAuthError(account_id, error,
                    /*fire_auth_error_changed=*/true);

    FireRefreshTokenAvailable(account_id);
  }
}

void FakeProfileOAuth2TokenServiceDelegate::RevokeCredentialsInternal(
    const CoreAccountId& account_id) {
  IssueRefreshTokenForUser(account_id, std::string()
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                                           ,
                           std::vector<uint8_t>()
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );
}

void FakeProfileOAuth2TokenServiceDelegate::ExtractCredentialsInternal(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  auto it = refresh_tokens_.find(account_id);
  CHECK(it != refresh_tokens_.end(), base::NotFatalUntil::M130);
  to_service->GetDelegate()->UpdateCredentials(account_id, it->second);
  RevokeCredentials(account_id);
}

scoped_refptr<network::SharedURLLoaderFactory>
FakeProfileOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return shared_factory_;
}

bool FakeProfileOAuth2TokenServiceDelegate::FixAccountErrorIfPossible() {
  return fix_account_if_possible_ ? fix_account_if_possible_.Run() : false;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
FakeProfileOAuth2TokenServiceDelegate::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>();
}
#endif
