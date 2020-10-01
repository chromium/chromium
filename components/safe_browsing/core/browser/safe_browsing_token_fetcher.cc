// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace safe_browsing {

namespace {

const char kAPIScope[] = "https://www.googleapis.com/auth/chrome-safe-browsing";

#if defined(OS_ANDROID)
const int kTimeoutDelayFromMilliseconds = 50;
#else
const int kTimeoutDelayFromMilliseconds = 1000;
#endif

}  // namespace

SafeBrowsingTokenFetcher::SafeBrowsingTokenFetcher(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager),
      requests_sent_(0),
      weak_ptr_factory_(this) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
}

SafeBrowsingTokenFetcher::~SafeBrowsingTokenFetcher() {
  for (auto& id_and_callback : callbacks_) {
    std::move(id_and_callback.second).Run(base::nullopt);
  }
}

void SafeBrowsingTokenFetcher::Start(signin::ConsentLevel consent_level,
                                     Callback callback) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  const int request_id = requests_sent_;
  requests_sent_++;
  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(consent_level);
  callbacks_[request_id] = std::move(callback);
  token_fetchers_[request_id] =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          account_id, "safe_browsing_service", {kAPIScope},
          base::BindOnce(&SafeBrowsingTokenFetcher::OnTokenFetched,
                         weak_ptr_factory_.GetWeakPtr(), request_id),
          signin::AccessTokenFetcher::Mode::kImmediate);
  base::PostDelayedTask(
      FROM_HERE, CreateTaskTraits(ThreadID::UI),
      base::BindOnce(&SafeBrowsingTokenFetcher::OnTokenTimeout,
                     weak_ptr_factory_.GetWeakPtr(), request_id),
      base::TimeDelta::FromMilliseconds(kTimeoutDelayFromMilliseconds));
}

void SafeBrowsingTokenFetcher::OnTokenFetched(
    int request_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.TokenFetcher.ErrorType",
                            error.state(), GoogleServiceAuthError::NUM_STATES);
  if (error.state() == GoogleServiceAuthError::NONE)
    Finish(request_id, access_token_info);
  else
    Finish(request_id, base::nullopt);
}

void SafeBrowsingTokenFetcher::OnTokenTimeout(int request_id) {
  Finish(request_id, base::nullopt);
}

void SafeBrowsingTokenFetcher::Finish(
    int request_id,
    base::Optional<signin::AccessTokenInfo> token_info) {
  if (callbacks_.contains(request_id)) {
    std::move(callbacks_[request_id]).Run(token_info);
  }

  token_fetchers_.erase(request_id);
  callbacks_.erase(request_id);
}

}  // namespace safe_browsing
