// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_DEVICE_ACCOUNTS_PROVIDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_DEVICE_ACCOUNTS_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"

enum AuthenticationErrorCategory {
  // Unknown errors.
  kAuthenticationErrorCategoryUnknownErrors,
  // Authorization errors.
  kAuthenticationErrorCategoryAuthorizationErrors,
  // Authorization errors with HTTP_FORBIDDEN (403) error code.
  kAuthenticationErrorCategoryAuthorizationForbiddenErrors,
  // Network server errors includes parsing error and should be treated as
  // transient/offline errors.
  kAuthenticationErrorCategoryNetworkServerErrors,
  // User cancellation errors should be handled by treating them as a no-op.
  kAuthenticationErrorCategoryUserCancellationErrors,
  // User identity not found errors.
  kAuthenticationErrorCategoryUnknownIdentityErrors,
};

// Interface that provides a mechanism for interacting with the underlying
// device accounts support.
class DeviceAccountsProvider {
 public:
  // Account information.
  struct AccountInfo {
    std::string gaia;
    std::string email;
    std::string hosted_domain;
  };

  // Access token info.
  struct AccessTokenInfo {
    std::string token;
    base::Time expiration_time;
  };

  // Result of GetAccessToken() passed to the callback. Contains either
  // a valid AccessTokenInfo or the error.
  using AccessTokenResult =
      base::expected<AccessTokenInfo, AuthenticationErrorCategory>;

  // Callback invoked when access token have been fetched.
  using AccessTokenCallback =
      base::OnceCallback<void(AccessTokenResult result)>;

  DeviceAccountsProvider() = default;
  virtual ~DeviceAccountsProvider() = default;

  // Returns the ids of all accounts.
  virtual std::vector<AccountInfo> GetAllAccounts() const;

  // Starts fetching an access token for the account with id |gaia_id| with
  // the given |scopes|. Once the token is obtained, |callback| is called.
  virtual void GetAccessToken(const std::string& gaia_id,
                              const std::string& client_id,
                              const std::set<std::string>& scopes,
                              AccessTokenCallback callback);
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_DEVICE_ACCOUNTS_PROVIDER_H_
