// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/profile_identity_provider.h"

#include "base/functional/bind.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

namespace invalidation {

namespace {

// `ActiveAccountAccessTokenFetcher` implementation that is backed by
// `IdentityManager` and wraps an `PrimaryAccountAccessTokenFetcher` internally.
class AccessTokenFetcherAdaptor : public ActiveAccountAccessTokenFetcher {
 public:
  AccessTokenFetcherAdaptor(const std::string& oauth_consumer_name,
                            signin::IdentityManager* identity_manager,
                            const signin::ScopeSet& scopes,
                            ActiveAccountAccessTokenCallback callback);
  AccessTokenFetcherAdaptor(const AccessTokenFetcherAdaptor& other) = delete;
  AccessTokenFetcherAdaptor& operator=(const AccessTokenFetcherAdaptor& other) =
      delete;
  ~AccessTokenFetcherAdaptor() override = default;

 private:
  // Invokes |callback_| with (|error|, |access_token_info.token|).
  void HandleTokenRequestCompletion(GoogleServiceAuthError error,
                                    signin::AccessTokenInfo access_token_info);

  ActiveAccountAccessTokenCallback callback_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      primary_account_access_token_fetcher_;
};

AccessTokenFetcherAdaptor::AccessTokenFetcherAdaptor(
    const std::string& oauth_consumer_name,
    signin::IdentityManager* identity_manager,
    const signin::ScopeSet& scopes,
    ActiveAccountAccessTokenCallback callback)
    : callback_(std::move(callback)) {
  primary_account_access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          oauth_consumer_name, identity_manager, scopes,
          base::BindOnce(
              &AccessTokenFetcherAdaptor::HandleTokenRequestCompletion,
              // It is safe to use base::Unretained as
              // |this| owns |access_token_fetcher_|.
              base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

void AccessTokenFetcherAdaptor::HandleTokenRequestCompletion(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  primary_account_access_token_fetcher_.reset();

  std::move(callback_).Run(error, access_token_info.token);
}

}  // namespace

ProfileIdentityProvider::ProfileIdentityProvider(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_->AddObserver(this);
}

ProfileIdentityProvider::~ProfileIdentityProvider() {
  identity_manager_->RemoveObserver(this);
}

CoreAccountId ProfileIdentityProvider::GetActiveAccountId() {
  return identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
}

bool ProfileIdentityProvider::IsActiveAccountWithRefreshToken() {
  if (GetActiveAccountId().empty() ||
      !identity_manager_->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    return false;
  }

  return true;
}

std::unique_ptr<ActiveAccountAccessTokenFetcher>
ProfileIdentityProvider::FetchAccessToken(
    const std::string& oauth_consumer_name,
    const signin::ScopeSet& scopes,
    ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<AccessTokenFetcherAdaptor>(
      oauth_consumer_name, identity_manager_, scopes, std::move(callback));
}

void ProfileIdentityProvider::InvalidateAccessToken(
    const signin::ScopeSet& scopes,
    const std::string& access_token) {
  identity_manager_->RemoveAccessTokenFromCache(GetActiveAccountId(), scopes,
                                                access_token);
}

void ProfileIdentityProvider::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kNone) {
    return;
  }

  const CoreAccountId& previous_account_id =
      event_details.GetPreviousState().primary_account.account_id;
  const CoreAccountId& current_account_id =
      event_details.GetCurrentState().primary_account.account_id;

  if (!previous_account_id.empty()) {
    FireOnActiveAccountLogout();
  }

  if (!current_account_id.empty()) {
    FireOnActiveAccountLogin();
  }
}

void ProfileIdentityProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  ProcessRefreshTokenUpdateForAccount(account_info.account_id);
}

}  // namespace invalidation
