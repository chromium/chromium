// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/api_access_token_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace supervised_user {
namespace {

base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
ToSingleReturnValue(GoogleServiceAuthError error,
                    signin::AccessTokenInfo access_token_info) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    return access_token_info;
  }
  return base::unexpected(error);
}

}  // namespace

ApiAccessTokenFetcher::ApiAccessTokenFetcher(
    signin::IdentityManager& identity_manager,
    const AccessTokenConfig& access_token_config)
    : identity_manager_(identity_manager),
      access_token_config_(access_token_config) {
  CHECK(!access_token_config.oauth2_scope.empty())
      << "OAuth2 scope is required";
  // base::Unretained(.) is safe, because no extra on-destroyed semantics are
  // needed and this instance must outlive the callback execution.
  CHECK(access_token_config_.mode.has_value())
      << "signin::PrimaryAccountAccessTokenFetcher::Mode is required";
}
ApiAccessTokenFetcher::~ApiAccessTokenFetcher() = default;

void ApiAccessTokenFetcher::GetToken(Consumer consumer) {
  OAuth2AccessTokenManager::ScopeSet scope_set(
      {std::string(access_token_config_.oauth2_scope)});
  primary_account_access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*oauth_consumer_name=*/"supervised_user_fetcher",
          &identity_manager_.get(), scope_set,
          base::BindOnce(&ApiAccessTokenFetcher::OnAccessTokenFetchComplete,
                         base::Unretained(this), std::move(consumer)),
          *(access_token_config_.mode), signin::ConsentLevel::kSignin);
}

void ApiAccessTokenFetcher::InvalidateToken() {
  CHECK(!access_token_info_.token.empty());
  identity_manager_->RemoveAccessTokenFromCache(
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      {std::string(access_token_config_.oauth2_scope)},
      access_token_info_.token);
}

void ApiAccessTokenFetcher::OnAccessTokenFetchComplete(
    Consumer consumer,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_info_ = access_token_info;
  std::move(consumer).Run(ToSingleReturnValue(error, access_token_info_));
}
}  // namespace supervised_user
