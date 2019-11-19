// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {

PrimaryAccountAccessTokenFetcher::PrimaryAccountAccessTokenFetcher(
    const std::string& oauth_consumer_name,
    IdentityManager* identity_manager,
    const identity::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    Mode mode)
    : oauth_consumer_name_(oauth_consumer_name),
      identity_manager_(identity_manager),
      scopes_(scopes),
      callback_(std::move(callback)),
      access_token_retried_(false),
      mode_(mode) {
  if (mode_ == Mode::kImmediate || AreCredentialsAvailable()) {
    StartAccessTokenRequest();
    return;
  }

  // Start observing the IdentityManager. This observer will be removed either
  // when credentials are obtained and an access token request is started or
  // when this object is destroyed.
  identity_manager_observer_.Add(identity_manager_);
}

PrimaryAccountAccessTokenFetcher::~PrimaryAccountAccessTokenFetcher() {}

bool PrimaryAccountAccessTokenFetcher::AreCredentialsAvailable() const {
  DCHECK_EQ(Mode::kWaitUntilAvailable, mode_);

  return identity_manager_->HasPrimaryAccountWithRefreshToken();
}

void PrimaryAccountAccessTokenFetcher::StartAccessTokenRequest() {
  DCHECK(mode_ == Mode::kImmediate || AreCredentialsAvailable());

  // By the time of starting an access token request, we should no longer be
  // listening for signin-related events.
  DCHECK(!identity_manager_observer_.IsObserving(identity_manager_));

  // Note: We might get here even in cases where we know that there's no refresh
  // token. We're requesting an access token anyway, so that the token service
  // will generate an appropriate error code that we can return to the client.
  DCHECK(!access_token_fetcher_);

  // NOTE: This class does not utilize AccessTokenFetcher in its
  // |kWaitUntilRefreshTokenAvailable| mode because the PAATF semantics specify
  // that when used in *its* |kWaitUntilAvailable| mode, the access token
  // request should be started when the account is primary AND has a refresh
  // token available. AccessTokenFetcher used in
  // |kWaitUntilRefreshTokenAvailable| mode would guarantee only the latter.
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      identity_manager_->GetPrimaryAccountId(), oauth_consumer_name_, scopes_,
      base::BindOnce(
          &PrimaryAccountAccessTokenFetcher::OnAccessTokenFetchComplete,
          base::Unretained(this)),
      AccessTokenFetcher::Mode::kImmediate);
}

void PrimaryAccountAccessTokenFetcher::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  ProcessSigninStateChange();
}

void PrimaryAccountAccessTokenFetcher::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  ProcessSigninStateChange();
}

void PrimaryAccountAccessTokenFetcher::ProcessSigninStateChange() {
  DCHECK_EQ(Mode::kWaitUntilAvailable, mode_);

  if (!AreCredentialsAvailable())
    return;

  identity_manager_observer_.Remove(identity_manager_);

  StartAccessTokenRequest();
}

void PrimaryAccountAccessTokenFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  // There is a special case for Android that RefreshTokenIsAvailable and
  // StartRequest are called to pre-fetch the account image and name before
  // sign-in. In that case, our ongoing access token request gets cancelled.
  // Moreover, OnRefreshTokenAvailable might happen after startup when the
  // credentials are changed/updated.
  // To handle these cases, we retry a canceled request once.
  // However, a request may also get cancelled for legitimate reasons, e.g.
  // because the user signed out. In those cases, there's no point in retrying,
  // so only retry if there (still) is a valid refresh token.
  // NOTE: Maybe we should retry for all transient errors here, so that clients
  // don't have to.
  if (mode_ == Mode::kWaitUntilAvailable && !access_token_retried_ &&
      error.state() == GoogleServiceAuthError::State::REQUEST_CANCELED &&
      AreCredentialsAvailable()) {
    access_token_retried_ = true;
    StartAccessTokenRequest();
    return;
  }

  // Per the contract of this class, it is allowed for consumers to delete this
  // object from within the callback that is run below. Hence, it is not safe to
  // add any code below this call.
  std::move(callback_).Run(std::move(error), std::move(access_token_info));
}

}  // namespace signin
