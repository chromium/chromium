// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_auth_token_provider.h"

#include "base/sequence_checker.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"

namespace plus_addresses {

PlusAddressAuthTokenProvider::PlusAddressAuthTokenProvider(
    signin::IdentityManager* identity_manager,
    const signin::ScopeSet& scopes)
    : identity_manager_(identity_manager), scopes_(scopes) {}

PlusAddressAuthTokenProvider::~PlusAddressAuthTokenProvider() = default;

void PlusAddressAuthTokenProvider::GetAuthToken(
    OnAuthTokenFetchedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Enqueue `callback` if the token is still being fetched.
  if (access_token_fetcher_) {
    pending_callbacks_.emplace(std::move(callback));
    return;
  }

  // TODO (kaklilu): Handle requests when token is nearing expiration.
  if (clock_->Now() < access_token_info_.expiration_time) {
    std::move(callback).Run(access_token_info_.token);
  } else {
    // Enqueue the callback and request a new token.
    pending_callbacks_.emplace(std::move(callback));
    RequestAuthToken();
  }
}

void PlusAddressAuthTokenProvider::RequestAuthToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"PlusAddressAuthTokenProvider", identity_manager_,
          scopes_,
          base::BindOnce(&PlusAddressAuthTokenProvider::OnTokenFetched,
                         // It is safe to use base::Unretained as
                         // |this| owns |access_token_fetcher_|.
                         base::Unretained(this)),
          // Use WaitUntilAvailable to defer getting an OAuth token until
          // the user is signed in. We can switch to kImmediate once we
          // have a sign in observer that guarantees we're already signed in
          // by this point.
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          // Sync doesn't need to be enabled for us to use PlusAddresses.
          signin::ConsentLevel::kSignin);
}

void PlusAddressAuthTokenProvider::OnTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    access_token_info_ = access_token_info;
    // Run stored callbacks.
    while (!pending_callbacks_.empty()) {
      std::move(pending_callbacks_.front()).Run(access_token_info.token);
      pending_callbacks_.pop();
    }
  } else {
    access_token_request_error_ = error;
    // TODO (kaklilu): Replace this log with Histogram of OAuth errors.
    VLOG(1) << "PlusAddressAuthTokenProvider failed to get OAuth token:"
            << error.ToString();
  }
}

}  // namespace plus_addresses
