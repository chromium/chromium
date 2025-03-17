// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr char kOauthConsumerName[] = "promotion_eligibility_checker";

constexpr char kPolicyPromotionBannerLocale[] = "en-US";

}  // namespace

namespace enterprise_promotion {

PromotionEligibilityChecker::PromotionEligibilityChecker(
    const std::string& profile_id,
    policy::CloudPolicyClient* client,
    signin::IdentityManager* identity_manager,
    std::string locale,
    bool dismissed_banner_pref)
    : identity_manager_(std::move(identity_manager)) {
  if (dismissed_banner_pref || locale != kPolicyPromotionBannerLocale) {
    return;
  }

  if (!client || !client->is_registered()) {
    return;
  }

  promotion_query_client_ = std::make_unique<policy::CloudPolicyClient>(
      profile_id, client->service(), client->GetURLLoaderFactory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());

  promotion_query_client_->SetupRegistration(
      client->dm_token(), client->client_id(), client->user_affiliation_ids());
}

PromotionEligibilityChecker::~PromotionEligibilityChecker() = default;

void PromotionEligibilityChecker::MaybeCheckPromotionEligibility(
    const CoreAccountId account_id,
    PromotionEligibilityChecker::PromotionEligibilityCallback callback) {
  DCHECK(!access_token_fetcher_);
  // The caller must supply a username.
  DCHECK(callback);
  callback_ = std::move(callback);

  if (account_id.empty() ||
      !identity_manager_->HasAccountWithRefreshToken(account_id) ||
      !promotion_query_client_ || !promotion_query_client_->is_registered()) {
    std::move(callback_).Run(
        enterprise_management::GetUserEligiblePromotionsResponse());
    return;
  }
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
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback_).Run(
        enterprise_management::GetUserEligiblePromotionsResponse());
    return;
  }

  oauth_token_ = token_info.token;

  if (oauth_token_.empty()) {
    std::move(callback_).Run(
        enterprise_management::GetUserEligiblePromotionsResponse());
    return;
  }

  CheckPromotionEligibilityWithAuthToken(oauth_token_);
}

void PromotionEligibilityChecker::CheckPromotionEligibilityWithAuthToken(
    std::string oauth_token) {
  promotion_query_client_->SetOAuthTokenAsAdditionalAuth(oauth_token);
  promotion_query_client_->DeterminePromotionEligibility(std::move(callback_));
}

void PromotionEligibilityChecker::SetCloudPolicyClientForTesting(
    std::unique_ptr<policy::CloudPolicyClient> testing_client) {
  promotion_query_client_ = std::move(testing_client);
}

}  // namespace enterprise_promotion
