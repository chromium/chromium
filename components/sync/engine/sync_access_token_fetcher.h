// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ACCESS_TOKEN_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "components/signin/public/identity_manager/access_token_info.h"

namespace syncer {

// Interface used by the sync engine (running on the sync sequence) to request
// an OAuth access token from the UI thread (where SyncAuthManager lives).
class SyncAccessTokenFetcher {
 public:
  virtual ~SyncAccessTokenFetcher() = default;

  virtual void FetchAccessToken(
      base::OnceCallback<void(signin::AccessTokenInfo)> callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ACCESS_TOKEN_FETCHER_H_
