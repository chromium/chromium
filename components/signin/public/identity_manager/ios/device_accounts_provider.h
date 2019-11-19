// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_DEVICE_ACCOUNTS_PROVIDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_DEVICE_ACCOUNTS_PROVIDER_H_

#if defined(__OBJC__)
@class NSDate;
@class NSError;
@class NSString;
#else
class NSDate;
class NSError;
class NSString;
#endif  // defined(__OBJC__)

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"

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

  using AccessTokenCallback = base::OnceCallback<
      void(NSString* token, NSDate* expiration, NSError* error)>;

  DeviceAccountsProvider() {}
  virtual ~DeviceAccountsProvider() {}

  // Returns the ids of all accounts.
  virtual std::vector<AccountInfo> GetAllAccounts() const;

  // Starts fetching an access token for the account with id |gaia_id| with
  // the given |scopes|. Once the token is obtained, |callback| is called.
  virtual void GetAccessToken(const std::string& gaia_id,
                              const std::string& client_id,
                              const std::set<std::string>& scopes,
                              AccessTokenCallback callback);

  // Returns the authentication error category of |error| associated with the
  // account with id |gaia_id|.
  virtual AuthenticationErrorCategory GetAuthenticationErrorCategory(
      const std::string& gaia_id,
      NSError* error) const;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_DEVICE_ACCOUNTS_PROVIDER_H_
