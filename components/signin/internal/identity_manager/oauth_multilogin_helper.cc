// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr int kMaxFetcherRetries = 3;

std::string FindTokenForAccount(
    const std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>&
        gaia_id_token_pairs,
    const std::string& gaia_id) {
  for (const auto& gaia_id_token : gaia_id_token_pairs) {
    if (gaia_id == gaia_id_token.gaia_id_)
      return gaia_id_token.token_;
  }
  return std::string();
}

CoreAccountId FindAccountIdForGaiaId(
    const std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair>& accounts,
    const std::string& gaia_id) {
  for (const auto& account : accounts) {
    if (gaia_id == account.second)
      return account.first;
  }
  return CoreAccountId();
}

}  // namespace

namespace signin {

OAuthMultiloginHelper::OAuthMultiloginHelper(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    gaia::MultiloginMode mode,
    const std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair>& accounts,
    const std::string& external_cc_result,
    base::OnceCallback<void(SetAccountsInCookieResult)> callback)
    : signin_client_(signin_client),
      token_service_(token_service),
      mode_(mode),
      accounts_(accounts),
      external_cc_result_(external_cc_result),
      callback_(std::move(callback)) {
  DCHECK(signin_client_);
  DCHECK(token_service_);
  DCHECK(!accounts_.empty());
  DCHECK(callback_);

#ifndef NDEBUG
  // Check that there is no duplicate accounts.
  std::set<GaiaCookieManagerService::AccountIdGaiaIdPair>
      accounts_no_duplicates(accounts_.begin(), accounts_.end());
  DCHECK_EQ(accounts_.size(), accounts_no_duplicates.size());
#endif

  StartFetchingTokens();
}

OAuthMultiloginHelper::~OAuthMultiloginHelper() = default;

void OAuthMultiloginHelper::StartFetchingTokens() {
  DCHECK(!token_fetcher_);
  DCHECK(gaia_id_token_pairs_.empty());
  std::vector<CoreAccountId> account_ids;
  for (const auto& account : accounts_)
    account_ids.push_back(account.first);

  token_fetcher_ = std::make_unique<OAuthMultiloginTokenFetcher>(
      signin_client_, token_service_, account_ids,
      base::BindOnce(&OAuthMultiloginHelper::OnAccessTokensSuccess,
                     base::Unretained(this)),
      base::BindOnce(&OAuthMultiloginHelper::OnAccessTokensFailure,
                     base::Unretained(this)));
}

void OAuthMultiloginHelper::OnAccessTokensSuccess(
    const std::vector<OAuthMultiloginTokenFetcher::AccountIdTokenPair>&
        account_token_pairs) {
  DCHECK(gaia_id_token_pairs_.empty());
  for (size_t index = 0; index < accounts_.size(); index++) {
    // OAuthMultiloginTokenFetcher should return the tokens in the same order
    // as the account_ids that was passed to it.
    DCHECK_EQ(accounts_[index].first, account_token_pairs[index].account_id);
    gaia_id_token_pairs_.emplace_back(accounts_[index].second,
                                      account_token_pairs[index].token);
  }
  DCHECK_EQ(gaia_id_token_pairs_.size(), accounts_.size());
  token_fetcher_.reset();

  signin_client_->DelayNetworkCall(
      base::BindOnce(&OAuthMultiloginHelper::StartFetchingMultiLogin,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OAuthMultiloginHelper::OnAccessTokensFailure(
    const GoogleServiceAuthError& error) {
  token_fetcher_.reset();
  std::move(callback_).Run(error.IsTransientError()
                               ? SetAccountsInCookieResult::kTransientError
                               : SetAccountsInCookieResult::kPersistentError);
  // Do not add anything below this line, because this may be deleted.
}

void OAuthMultiloginHelper::StartFetchingMultiLogin() {
  DCHECK_EQ(gaia_id_token_pairs_.size(), accounts_.size());
  gaia_auth_fetcher_ =
      signin_client_->CreateGaiaAuthFetcher(this, gaia::GaiaSource::kChrome);
  gaia_auth_fetcher_->StartOAuthMultilogin(mode_, gaia_id_token_pairs_,
                                           external_cc_result_);
}

void OAuthMultiloginHelper::OnOAuthMultiloginFinished(
    const OAuthMultiloginResult& result) {
  if (result.status() == OAuthMultiloginResponseStatus::kOk) {
    std::vector<std::string> account_ids;
    for (const auto& account : accounts_)
      account_ids.push_back(account.first.id);
    VLOG(1) << "Multilogin successful accounts="
            << base::JoinString(account_ids, " ");
    StartSettingCookies(result);
    return;
  }

  // If Gaia responded with kInvalidTokens, we have to mark tokens as invalid.
  if (result.status() == OAuthMultiloginResponseStatus::kInvalidTokens) {
    for (const std::string& failed_gaia_id : result.failed_gaia_ids()) {
      std::string failed_token =
          FindTokenForAccount(gaia_id_token_pairs_, failed_gaia_id);
      if (failed_token.empty()) {
        LOG(ERROR)
            << "Unexpected failed token for account not present in request: "
            << failed_gaia_id;
        continue;
      }
      token_service_->InvalidateTokenForMultilogin(
          FindAccountIdForGaiaId(accounts_, failed_gaia_id), failed_token);
    }
  }

  bool is_transient_error =
      result.status() == OAuthMultiloginResponseStatus::kInvalidTokens ||
      result.status() == OAuthMultiloginResponseStatus::kRetry;

  if (is_transient_error && ++fetcher_retries_ < kMaxFetcherRetries) {
    gaia_id_token_pairs_.clear();
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
      signin_client_->GetCookieManager();
  const std::vector<net::CanonicalCookie>& cookies = result.cookies();

  for (const net::CanonicalCookie& cookie : cookies) {
    cookies_to_set_.insert(std::make_pair(cookie.Name(), cookie.Domain()));
  }
  for (const net::CanonicalCookie& cookie : cookies) {
    if (cookies_to_set_.find(std::make_pair(cookie.Name(), cookie.Domain())) !=
        cookies_to_set_.end()) {
      base::OnceCallback<void(net::CanonicalCookie::CookieInclusionStatus)>
          callback = base::BindOnce(&OAuthMultiloginHelper::OnCookieSet,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    cookie.Name(), cookie.Domain());
      net::CookieOptions options;
      options.set_include_httponly();
      // Permit it to set a SameSite cookie if it wants to.
      options.set_same_site_cookie_context(
          net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
      cookie_manager->SetCanonicalCookie(
          cookie, "https", options,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback),
              net::CanonicalCookie::CookieInclusionStatus(
                  net::CanonicalCookie::CookieInclusionStatus::
                      EXCLUDE_UNKNOWN_ERROR)));
    } else {
      LOG(ERROR) << "Duplicate cookie found: " << cookie.Name() << " "
                 << cookie.Domain();
    }
  }
}

void OAuthMultiloginHelper::OnCookieSet(
    const std::string& cookie_name,
    const std::string& cookie_domain,
    net::CanonicalCookie::CookieInclusionStatus status) {
  cookies_to_set_.erase(std::make_pair(cookie_name, cookie_domain));
  bool success = status.IsInclude();
  if (!success) {
    LOG(ERROR) << "Failed to set cookie " << cookie_name
               << " for domain=" << cookie_domain << ".";
  }
  UMA_HISTOGRAM_BOOLEAN("Signin.SetCookieSuccess", success);
  if (cookies_to_set_.empty())
    std::move(callback_).Run(SetAccountsInCookieResult::kSuccess);
  // Do not add anything below this line, because this may be deleted.
}

}  // namespace signin
