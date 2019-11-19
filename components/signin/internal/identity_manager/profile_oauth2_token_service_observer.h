// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_OBSERVER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_OBSERVER_H_

#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

// Classes that want to listen for refresh token availability should
// implement this interface and register with the ProfileOAuth2TokenService::
// AddObserver() call.
class ProfileOAuth2TokenServiceObserver {
 public:
  // Called whenever a new login-scoped refresh token is available for
  // account |account_id|. Once available, access tokens can be retrieved for
  // this account. This is called during initial startup for each token
  // loaded (and any time later when, e.g., credentials change). When called,
  // any pending token request is canceled and needs to be retried. Such a
  // pending request can easily occur on Android, where refresh tokens are
  // held by the OS and are thus often available on startup even before
  // OnRefreshTokenAvailable() is called.
  virtual void OnRefreshTokenAvailable(const CoreAccountId& account_id) {}
  // Called whenever the login-scoped refresh token becomes unavailable for
  // account |account_id|.
  virtual void OnRefreshTokenRevoked(const CoreAccountId& account_id) {}
  // Called after all refresh tokens are loaded during ProfileOAuth2TokenService
  // startup.
  virtual void OnRefreshTokensLoaded() {}
  // Sent after a batch of refresh token changes is done.
  virtual void OnEndBatchChanges() {}
  // Called when the authentication error state for |account_id| has changed.
  // Note: It is always called after |OnRefreshTokenAvailable| when refresh
  // token is updated. It is not called when the refresh token is revoked.
  virtual void OnAuthErrorChanged(const CoreAccountId& account_id,
                                  const GoogleServiceAuthError& auth_error) {}

 protected:
  virtual ~ProfileOAuth2TokenServiceObserver() {}
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_OBSERVER_H_
