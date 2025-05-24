// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_response.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/session_binding_utils.h"
#endif

namespace signin {

namespace {

constexpr int kMaxFetcherRetries = 3;
static_assert(kMaxFetcherRetries > 1, "Must have at least one retry attempt");

CoreAccountId FindAccountIdForGaiaId(
    const std::vector<OAuthMultiloginHelper::AccountIdGaiaIdPair>& accounts,
    const GaiaId& gaia_id) {
  auto it = std::ranges::find(
      accounts, gaia_id, &OAuthMultiloginHelper::AccountIdGaiaIdPair::second);
  return it != accounts.end() ? it->first : CoreAccountId();
}

std::string FindTokenForAccountId(
    const base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse>& tokens,
    const CoreAccountId& account_id) {
  auto it = tokens.find(account_id);
  return it != tokens.end() ? it->second.oauth_token() : std::string();
}

}  // namespace

OAuthMultiloginHelper::OAuthMultiloginHelper(
    SigninClient* signin_client,
    AccountsCookieMutator::PartitionDelegate* partition_delegate,
    ProfileOAuth2TokenService* token_service,
    gaia::MultiloginMode mode,
    const std::vector<AccountIdGaiaIdPair>& accounts,
    const std::string& external_cc_result,
    const gaia::GaiaSource& gaia_source,
    base::OnceCallback<void(SetAccountsInCookieResult)> callback)
    : signin_client_(signin_client),
      partition_delegate_(partition_delegate),
      token_service_(token_service),
      mode_(mode),
      accounts_(accounts),
      external_cc_result_(external_cc_result),
      gaia_source_(gaia_source),
      callback_(std::move(callback)) {
  DCHECK(signin_client_);
  DCHECK(partition_delegate_);
  DCHECK(token_service_);
  DCHECK(!accounts_.empty());
  DCHECK(callback_);

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  bound_session_delegate_ =
      signin_client_->CreateBoundSessionOAuthMultiloginDelegate();
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#ifndef NDEBUG
  // Check that there is no duplicate accounts.
  std::set<AccountIdGaiaIdPair> accounts_no_duplicates(accounts_.begin(),
                                                       accounts_.end());
  DCHECK_EQ(accounts_.size(), accounts_no_duplicates.size());
#endif

  StartFetchingTokens();
}

OAuthMultiloginHelper::~OAuthMultiloginHelper() = default;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
void OAuthMultiloginHelper::SetEphemeralKeyForTesting(
    HybridEncryptionKey ephemeral_key) {
  ephemeral_key_ = std::move(ephemeral_key);
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

void OAuthMultiloginHelper::StartFetchingTokens() {
  DCHECK(!token_fetcher_);
  DCHECK(tokens_.empty());
  std::vector<OAuthMultiloginTokenFetcher::AccountParams> account_params;
  for (const auto& account : accounts_) {
    const CoreAccountId& account_id = account.first;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    auto challenge_it = token_binding_challenges_.find(account_id);
    bool has_challenge = challenge_it != token_binding_challenges_.end();
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    account_params.push_back(
        {.account_id = account_id
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
         ,
         .token_binding_challenge =
             has_challenge ? challenge_it->second : std::string()
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        });
  }

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::string ephemeral_public_key;
  if (!token_binding_challenges_.empty()) {
    // Create a new key if we don't have one.
    if (!ephemeral_key_.has_value()) {
      ephemeral_key_.emplace();
    }
    ephemeral_public_key = ephemeral_key_->ExportPublicKey();
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  token_fetcher_ = std::make_unique<OAuthMultiloginTokenFetcher>(
      signin_client_, token_service_, std::move(account_params),
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      std::move(ephemeral_public_key),
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      base::BindOnce(&OAuthMultiloginHelper::OnMultiloginTokensSuccess,
                     base::Unretained(this)),
      base::BindOnce(&OAuthMultiloginHelper::OnMultiloginTokensFailure,
                     base::Unretained(this)));
}

void OAuthMultiloginHelper::OnMultiloginTokensSuccess(
    base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse> tokens) {
  CHECK(tokens_.empty());
  CHECK_EQ(tokens.size(), accounts_.size());
  tokens_ = std::move(tokens);
  token_fetcher_.reset();
  signin_client_->DelayNetworkCall(
      base::BindOnce(&OAuthMultiloginHelper::StartFetchingMultiLogin,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OAuthMultiloginHelper::OnMultiloginTokensFailure(
    const GoogleServiceAuthError& error) {
  token_fetcher_.reset();
  std::move(callback_).Run(error.IsTransientError()
                               ? SetAccountsInCookieResult::kTransientError
                               : SetAccountsInCookieResult::kPersistentError);
  // Do not add anything below this line, because this may be deleted.
}

void OAuthMultiloginHelper::StartFetchingMultiLogin() {
  CHECK_EQ(tokens_.size(), accounts_.size());
  std::vector<gaia::MultiloginAccountAuthCredentials> multilogin_credentials;
  // Accounts must be listed in the same order as in `accounts_`.
  for (const auto& account : accounts_) {
    auto token_it = tokens_.find(account.first);
    CHECK(token_it != tokens_.end());
    std::string token_binding_assertion;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    token_binding_assertion = token_it->second.token_binding_assertion();
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

    multilogin_credentials.emplace_back(account.second,
                                        token_it->second.oauth_token(),
                                        std::move(token_binding_assertion));
  }

  OAuthMultiloginResult::CookieDecryptor decryptor;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (ephemeral_key_.has_value()) {
    decryptor = base::BindRepeating(&DecryptValueWithEphemeralKey,
                                    std::move(ephemeral_key_).value());
    // std::move() above doesn't invalidate `ephemeral_key_`, so call reset()
    // explicitly.
    ephemeral_key_.reset();
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  gaia_auth_fetcher_ = partition_delegate_->CreateGaiaAuthFetcherForPartition(
      this, gaia_source_);
  gaia_auth_fetcher_->StartOAuthMultilogin(
      mode_, multilogin_credentials, external_cc_result_, std::move(decryptor));
}

void OAuthMultiloginHelper::OnOAuthMultiloginFinished(
    const OAuthMultiloginResult& result) {
  if (result.status() == OAuthMultiloginResponseStatus::kOk) {
    if (VLOG_IS_ON(1)) {
      std::vector<std::string> account_ids;
      for (const auto& account : accounts_) {
        account_ids.push_back(account.first.ToString());
      }
      VLOG(1) << "Multilogin successful accounts="
              << base::JoinString(account_ids, " ");
    }
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    if (bound_session_delegate_) {
      bound_session_delegate_->BeforeSetCookies(result);
    }
#endif

    StartSettingCookies(result);
    return;
  }

  // If Gaia responded with kInvalidTokens or kRetryWithTokenBindingChallenge,
  // we have to mark tokens without recovery method as invalid.
  if (result.status() == OAuthMultiloginResponseStatus::kInvalidTokens ||
      result.status() ==
          OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge) {
    for (const OAuthMultiloginResult::FailedAccount& failed_account :
         result.failed_accounts()) {
      CoreAccountId failed_account_id =
          FindAccountIdForGaiaId(accounts_, failed_account.gaia_id);
      if (failed_account_id.empty()) {
        LOG(ERROR) << "Unexpected failed gaia id for an account not present in "
                      "request: "
                   << failed_account.gaia_id;
        continue;
      }
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      if (!failed_account.token_binding_challenge.empty()) {
        auto [_, inserted] = token_binding_challenges_.insert(
            {failed_account_id, failed_account.token_binding_challenge});
        if (inserted) {
          // If an account haven't received a token binding challenge before,
          // try to recover by providing a token binding assertion.
          continue;
        }
      }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

      std::string failed_token =
          FindTokenForAccountId(tokens_, failed_account_id);
      CHECK(!failed_token.empty());
      token_service_->InvalidateTokenForMultilogin(failed_account_id,
                                                   failed_token);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      token_binding_challenges_.erase(failed_account_id);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    }
  }

  bool is_transient_error =
      result.status() == OAuthMultiloginResponseStatus::kInvalidTokens ||
      result.status() == OAuthMultiloginResponseStatus::kRetry ||
      result.status() ==
          OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge;

  if (is_transient_error && ++fetcher_retries_ < kMaxFetcherRetries) {
    tokens_.clear();
    StartFetchingTokens();
    return;
  }
  std::move(callback_).Run(is_transient_error
                               ? SetAccountsInCookieResult::kTransientError
                               : SetAccountsInCookieResult::kPersistentError);
  // Do not add anything below this line, because this may be deleted.
}

void OAuthMultiloginHelper::StartSettingCookies(
    const OAuthMultiloginResult& result) {
  DCHECK(cookies_to_set_.empty());
  network::mojom::CookieManager* cookie_manager =
      partition_delegate_->GetCookieManagerForPartition();
  const std::vector<net::CanonicalCookie>& cookies = result.cookies();

  for (const net::CanonicalCookie& cookie : cookies) {
    cookies_to_set_.insert(std::make_pair(cookie.Name(), cookie.Domain()));
  }
  for (const net::CanonicalCookie& cookie : cookies) {
    if (cookies_to_set_.find(std::make_pair(cookie.Name(), cookie.Domain())) !=
        cookies_to_set_.end()) {
      base::OnceCallback<void(net::CookieAccessResult)> callback =
          base::BindOnce(&OAuthMultiloginHelper::OnCookieSet,
                         weak_ptr_factory_.GetWeakPtr(), cookie.Name(),
                         cookie.Domain());
      net::CookieOptions options;
      options.set_include_httponly();
      // Permit it to set a SameSite cookie if it wants to.
      options.set_same_site_cookie_context(
          net::CookieOptions::SameSiteCookieContext::MakeInclusive());
      net::CookieInclusionStatus cookie_inclusion_status;
      cookie_inclusion_status.AddExclusionReason(
          net::CookieInclusionStatus::ExclusionReason::EXCLUDE_UNKNOWN_ERROR);
      cookie_manager->SetCanonicalCookie(
          cookie, net::cookie_util::SimulatedCookieSource(cookie, "https"),
          options,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback),
              net::CookieAccessResult(cookie_inclusion_status)));
    } else {
      LOG(ERROR) << "Duplicate cookie found: " << cookie.Name() << " "
                 << cookie.Domain();
    }
  }
}

void OAuthMultiloginHelper::OnCookieSet(const std::string& cookie_name,
                                        const std::string& cookie_domain,
                                        net::CookieAccessResult access_result) {
  cookies_to_set_.erase(std::make_pair(cookie_name, cookie_domain));
  bool success = access_result.status.IsInclude();
  if (!success) {
    LOG(ERROR) << "Failed to set cookie " << cookie_name
               << " for domain=" << cookie_domain << ".";
  }
  UMA_HISTOGRAM_BOOLEAN("Signin.SetCookieSuccess", success);
  if (cookies_to_set_.empty()) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    if (bound_session_delegate_) {
      bound_session_delegate_->OnCookiesSet();
    }
#endif
    std::move(callback_).Run(SetAccountsInCookieResult::kSuccess);
  }
  // Do not add anything below this line, because this may be deleted.
}

}  // namespace signin
