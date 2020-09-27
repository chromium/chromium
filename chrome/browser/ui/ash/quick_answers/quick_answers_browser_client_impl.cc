// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/quick_answers_browser_client_impl.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"

namespace {

constexpr char kCloudTranslationScope[] =
    "https://www.googleapis.com/auth/cloud-translation";

constexpr base::TimeDelta kMinTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(1000);
constexpr base::TimeDelta kMaxTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(60 * 1000);

}  // namespace

QuickAnswersBrowserClientImpl::QuickAnswersBrowserClientImpl() = default;

QuickAnswersBrowserClientImpl::~QuickAnswersBrowserClientImpl() = default;

void QuickAnswersBrowserClientImpl::RequestAccessToken(
    AccessTokenCallback callback) {
  if (!access_token_.empty()) {
    // Return the token if there is enough time to use the access token when
    // requested.
    if (expiration_time_ - base::Time::Now() > token_usage_time_buffer_) {
      std::move(callback).Run(access_token_);
      return;
    }

    access_token_.clear();
    expiration_time_ = base::Time::Now();
  }

  callbacks_.emplace_back(std::move(callback));

  // There is already pending request.
  if (access_token_fetcher_)
    return;

  RefreshAccessToken();
}

void QuickAnswersBrowserClientImpl::RefreshAccessToken() {
  auto* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager);

  CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  const signin::ScopeSet scopes{kCloudTranslationScope};
  DCHECK(!access_token_fetcher_);
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_info.account_id, /*oauth_consumer_name=*/"cros_quick_answers",
      scopes,
      base::BindOnce(&QuickAnswersBrowserClientImpl::OnAccessTokenRefreshed,
                     weak_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void QuickAnswersBrowserClientImpl::OnAccessTokenRefreshed(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // It's safe to delete AccessTokenFetcher from inside its own callback.
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    RetryRefreshAccessToken();
    return;
  }

  VLOG(1) << "Access token fetched.";
  access_token_ = access_token_info.token;
  expiration_time_ = access_token_info.expiration_time;
  NotifyAccessTokenRefreshed();
}

void QuickAnswersBrowserClientImpl::RetryRefreshAccessToken() {
  base::TimeDelta backoff_delay =
      std::min(kMinTokenRefreshDelay *
                   (1 << (token_refresh_error_backoff_factor_ - 1)),
               kMaxTokenRefreshDelay) +
      base::RandDouble() * kMinTokenRefreshDelay;

  if (backoff_delay < kMaxTokenRefreshDelay)
    ++token_refresh_error_backoff_factor_;

  token_refresh_timer_.Start(
      FROM_HERE, backoff_delay,
      base::BindOnce(&QuickAnswersBrowserClientImpl::RefreshAccessToken,
                     weak_factory_.GetWeakPtr()));
}

void QuickAnswersBrowserClientImpl::NotifyAccessTokenRefreshed() {
  std::vector<AccessTokenCallback> callbacks;
  callbacks.swap(callbacks_);

  for (auto& callback : callbacks)
    std::move(callback).Run(access_token_);
}
