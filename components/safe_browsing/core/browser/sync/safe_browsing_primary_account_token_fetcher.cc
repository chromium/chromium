// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace safe_browsing {

SafeBrowsingPrimaryAccountTokenFetcher::SafeBrowsingPrimaryAccountTokenFetcher(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager),
      weak_ptr_factory_(this) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
}

SafeBrowsingPrimaryAccountTokenFetcher::
    ~SafeBrowsingPrimaryAccountTokenFetcher() = default;

void SafeBrowsingPrimaryAccountTokenFetcher::Start(
    Callback callback) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));

  // NOTE: base::Unretained() is safe below as this object owns
  // |token_fetch_tracker_|, and the callback will not be invoked after
  // |token_fetch_tracker_| is destroyed.
  const int request_id = token_fetch_tracker_.StartTrackingTokenFetch(
      std::move(callback),
      base::BindOnce(&SafeBrowsingPrimaryAccountTokenFetcher::OnTokenTimeout,
                     base::Unretained(this)));
  CoreAccountId account_id = identity_manager_->GetPrimaryAccountId(
      signin::ConsentLevel::kNotRequired);
  token_fetchers_[request_id] =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          account_id, "safe_browsing_service", {kAPIScope},
          base::BindOnce(
              &SafeBrowsingPrimaryAccountTokenFetcher::OnTokenFetched,
              weak_ptr_factory_.GetWeakPtr(), request_id),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void SafeBrowsingPrimaryAccountTokenFetcher::OnTokenFetched(
    int request_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.TokenFetcher.ErrorType",
                            error.state(), GoogleServiceAuthError::NUM_STATES);
  if (error.state() == GoogleServiceAuthError::NONE)
    token_fetch_tracker_.OnTokenFetchComplete(request_id,
                                              access_token_info.token);
  else
    token_fetch_tracker_.OnTokenFetchComplete(request_id, std::string());

  token_fetchers_.erase(request_id);
}

void SafeBrowsingPrimaryAccountTokenFetcher::OnTokenTimeout(int request_id) {
  token_fetchers_.erase(request_id);
}

}  // namespace safe_browsing
