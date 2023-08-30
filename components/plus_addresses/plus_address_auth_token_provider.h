// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_AUTH_TOKEN_PROVIDER_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_AUTH_TOKEN_PROVIDER_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/default_clock.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace base {
class Clock;
}

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace plus_addresses {

using OnAuthTokenFetchedCallback = base::OnceCallback<void(std::string)>;

// Utility class for fetching an OAuth token for plus addresses to use when
// making requests to the plus-address server.
class PlusAddressAuthTokenProvider {
 public:
  PlusAddressAuthTokenProvider(signin::IdentityManager* identity_manager,
                               const signin::ScopeSet& scopes);
  ~PlusAddressAuthTokenProvider();

  // Runs `on_fetched` with the OAuth token once it is available.
  void GetAuthToken(OnAuthTokenFetchedCallback on_fetched);

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

 private:
  // Initiates a network request for an OAuth token, and may only be
  // called by GetAuthToken. This also must be run on the UI thread.
  void RequestAuthToken();
  void OnTokenFetched(GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);

  // The IdentityManager instance for the signed-in user.
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_ GUARDED_BY_CONTEXT(sequence_checker_);

  signin::AccessTokenInfo access_token_info_;
  GoogleServiceAuthError access_token_request_error_;
  signin::ScopeSet scopes_;
  // Stores callbacks to be run once the OAuth token is retrieved.
  base::queue<OnAuthTokenFetchedCallback> pending_callbacks_;
  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_AUTH_TOKEN_PROVIDER_H_
