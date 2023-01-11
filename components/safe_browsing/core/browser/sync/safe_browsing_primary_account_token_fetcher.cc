// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace safe_browsing {

SafeBrowsingPrimaryAccountTokenFetcher::SafeBrowsingPrimaryAccountTokenFetcher(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager),
      weak_ptr_factory_(this) {
}

SafeBrowsingPrimaryAccountTokenFetcher::
    ~SafeBrowsingPrimaryAccountTokenFetcher() = default;

void SafeBrowsingPrimaryAccountTokenFetcher::Start(
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // NOTE: When a token fetch timeout occurs |token_fetch_tracker_| will invoke
  // the client callback, which may end up synchronously destroying this object
  // before this object's own callback is invoked. Hence we bind our own
  // callback via a WeakPtr.
  const int request_id = token_fetch_tracker_.StartTrackingTokenFetch(
      std::move(callback),
      base::BindOnce(&SafeBrowsingPrimaryAccountTokenFetcher::OnTokenTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  token_fetchers_[request_id] =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          account_id, "safe_browsing_service",
          {GaiaConstants::kChromeSafeBrowsingOAuth2Scope},
          base::BindOnce(
              &SafeBrowsingPrimaryAccountTokenFetcher::OnTokenFetched,
              weak_ptr_factory_.GetWeakPtr(), request_id),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void SafeBrowsingPrimaryAccountTokenFetcher::OnInvalidAccessToken(
    const std::string& invalid_access_token) {
  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  identity_manager_->RemoveAccessTokenFromCache(
      account_id, {GaiaConstants::kChromeSafeBrowsingOAuth2Scope},
      invalid_access_token);
}

void SafeBrowsingPrimaryAccountTokenFetcher::OnTokenFetched(
    int request_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.TokenFetcher.ErrorType",
                            error.state(), GoogleServiceAuthError::NUM_STATES);
  DCHECK(token_fetchers_.contains(request_id));
  token_fetchers_.erase(request_id);

  if (error.state() == GoogleServiceAuthError::NONE)
    token_fetch_tracker_.OnTokenFetchComplete(request_id,
                                              access_token_info.token);
  else
    token_fetch_tracker_.OnTokenFetchComplete(request_id, std::string());

  // NOTE: Calling SafeBrowsingTokenFetchTracker::OnTokenFetchComplete might
  // have resulted in the synchronous destruction of this object, so there is
  // nothing safe to do here but return.
}

void SafeBrowsingPrimaryAccountTokenFetcher::OnTokenTimeout(int request_id) {
  DCHECK(token_fetchers_.contains(request_id));
  token_fetchers_.erase(request_id);
}

}  // namespace safe_browsing
