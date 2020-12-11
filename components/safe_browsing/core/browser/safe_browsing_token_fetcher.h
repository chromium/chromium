// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class IdentityManager;
class AccessTokenFetcher;
}  // namespace signin

namespace safe_browsing {

// This class is used to fetch access tokens for communcations with Safe
// Browsing. It asynchronously returns the access token for the current
// primary account, or nullopt if an error occurred. This must be
// run on the UI thread.
class SafeBrowsingTokenFetcher {
 public:
  using Callback =
      base::OnceCallback<void(base::Optional<signin::AccessTokenInfo>)>;

  // Create a SafeBrowsingTokenFetcher for the primary account of
  // |identity_manager|. |identity_manager| is unowned, and must outlive this
  // object.
  explicit SafeBrowsingTokenFetcher(signin::IdentityManager* identity_manager);

  ~SafeBrowsingTokenFetcher();

  // Begin fetching a token for the account with the given |consent_level|. The
  // result will be returned in |callback|. Must be called on the UI thread.
  void Start(signin::ConsentLevel consent_level, Callback callback);

 private:
  void OnTokenFetched(int request_id,
                      GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);
  void OnTokenTimeout(int request_id);
  void Finish(int request_id,
              base::Optional<signin::AccessTokenInfo> token_info);

  // Reference to the identity manager to fetch from.
  signin::IdentityManager* identity_manager_;

  // The count of requests sent. This is used as an ID for requests.
  int requests_sent_;

  // Active fetchers, keyed by ID.
  base::flat_map<int, std::unique_ptr<signin::AccessTokenFetcher>>
      token_fetchers_;

  // Active callbacks, keyed by ID.
  base::flat_map<int, Callback> callbacks_;

  base::WeakPtrFactory<SafeBrowsingTokenFetcher> weak_ptr_factory_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_
