// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"

namespace safe_browsing {

// This interface is used to fetch access tokens for communcations with Safe
// Browsing. It asynchronously returns an access token for the current account
// (as determined in concrete implementations), or nullopt if an error occurred.
// This must be run on the UI thread.
class SafeBrowsingTokenFetcher {
 public:
  using Callback =
      base::OnceCallback<void(base::Optional<signin::AccessTokenInfo>)>;

  virtual ~SafeBrowsingTokenFetcher() = default;

  // Begin fetching a token for the account with the given |consent_level|. The
  // result will be returned in |callback|. Must be called on the UI thread.
  virtual void Start(signin::ConsentLevel consent_level, Callback callback) = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_
