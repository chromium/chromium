// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {

constexpr char kOauthConsumerName[] = "promotion_eligibility_checker";

}  // namespace

namespace enterprise_promotion {

PromotionEligibilityChecker::PromotionEligibilityChecker(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

PromotionEligibilityChecker::~PromotionEligibilityChecker() = default;

void PromotionEligibilityChecker::FetchAccessToken(
    const CoreAccountId account_id) {
  DCHECK(!access_token_fetcher_);
  // The caller must supply a username.
  DCHECK(!account_id.empty());
  DCHECK(identity_manager_->HasAccountWithRefreshToken(account_id));

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);

  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_id, kOauthConsumerName, scopes,
      base::BindOnce(&PromotionEligibilityChecker::OnAuthTokenFetched,
                     weak_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void PromotionEligibilityChecker::OnAuthTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  oauth_token_ = token_info.token;
}

std::string PromotionEligibilityChecker::GetFetchedTokenForTesting() {
  return oauth_token_;
}

}  // namespace enterprise_promotion
