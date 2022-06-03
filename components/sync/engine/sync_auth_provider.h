// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_AUTH_PROVIDER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_AUTH_PROVIDER_H_

#include <string>

#include "base/callback.h"

class GoogleServiceAuthError;

namespace syncer {

// SyncAuthProvider is interface to access token related functions from sync
// engine.
class SyncAuthProvider {
 public:
  using RequestTokenCallback =
      base::OnceCallback<void(const GoogleServiceAuthError& error,
                              const std::string& token)>;

  virtual ~SyncAuthProvider() {}

  // Request access token for sync. Callback will be called with error and
  // access token. If error is anything other than NONE then token is invalid.
  virtual void RequestAccessToken(RequestTokenCallback callback) = 0;

  // Invalidate access token that was rejected by sync server.
  virtual void InvalidateAccessToken(const std::string& token) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_AUTH_PROVIDER_H_
