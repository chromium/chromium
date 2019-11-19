// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_FAKE_DEVICE_ACCOUNTS_PROVIDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_FAKE_DEVICE_ACCOUNTS_PROVIDER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

// Mock class of DeviceAccountsProvider for testing.
class FakeDeviceAccountsProvider : public DeviceAccountsProvider {
 public:
  FakeDeviceAccountsProvider();
  ~FakeDeviceAccountsProvider() override;

  // DeviceAccountsProvider
  void GetAccessToken(const std::string& account_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAllAccounts() const override;
  AuthenticationErrorCategory GetAuthenticationErrorCategory(
      const std::string& gaia_id,
      NSError* error) const override;

  // Methods to configure this fake provider.
  AccountInfo AddAccount(const std::string& gaia, const std::string& email);
  void ClearAccounts();

  // Issues access token responses.
  void IssueAccessTokenForAllRequests();
  void IssueAccessTokenErrorForAllRequests();

 private:
  using AccessTokenRequest = std::pair<std::string, AccessTokenCallback>;

  std::vector<AccountInfo> accounts_;
  std::vector<AccessTokenRequest> requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceAccountsProvider);
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_FAKE_DEVICE_ACCOUNTS_PROVIDER_H_
