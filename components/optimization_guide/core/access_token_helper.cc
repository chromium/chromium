// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/access_token_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace optimization_guide {

namespace {

constexpr char kOptGuideOAuthConsumerName[] = "optimization_guide";

void RecordAccessTokenResultHistogram(
    OptimizationGuideAccessTokenResult result) {
  base::UmaHistogramEnumeration("OptimizationGuide.AccessTokenHelper.Result",
                                result);
}

// Invoked when access token is ready.
void OnAccessTokenRequestCompleted(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>,
    AccessTokenReceivedCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_EQ(access_token_info.token.empty(),
            error.state() != GoogleServiceAuthError::NONE);
  std::move(callback).Run(access_token_info.token);

  if (error.state() == GoogleServiceAuthError::NONE) {
    RecordAccessTokenResultHistogram(
        OptimizationGuideAccessTokenResult::kSuccess);
  } else if (error.IsTransientError()) {
    RecordAccessTokenResultHistogram(
        OptimizationGuideAccessTokenResult::kTransientError);
  } else {
    RecordAccessTokenResultHistogram(
        OptimizationGuideAccessTokenResult::kPersistentError);
  }
}

}  // namespace

void RequestAccessToken(signin::IdentityManager* identity_manager,
                        const std::set<std::string>& oauth_scopes,
                        AccessTokenReceivedCallback callback) {
  DCHECK(!oauth_scopes.empty());
  if (!identity_manager) {
    RecordAccessTokenResultHistogram(
        OptimizationGuideAccessTokenResult::kUserNotSignedIn);
    std::move(callback).Run(std::string());
    return;
  }
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    RecordAccessTokenResultHistogram(
        OptimizationGuideAccessTokenResult::kUserNotSignedIn);
    std::move(callback).Run(std::string());
    return;
  }
  auto access_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kOptGuideOAuthConsumerName, identity_manager, oauth_scopes,
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
  auto* access_token_fetcher_ptr = access_token_fetcher.get();
  access_token_fetcher_ptr->Start(
      base::BindOnce(&OnAccessTokenRequestCompleted,
                     std::move(access_token_fetcher), std::move(callback)));
}

}  // namespace optimization_guide
