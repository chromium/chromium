// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_FAKE_DEVICE_ACCOUNTS_PROVIDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_FAKE_DEVICE_ACCOUNTS_PROVIDER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

// Mock class of DeviceAccountsProvider for testing.
class FakeDeviceAccountsProvider : public DeviceAccountsProvider {
 public:
  FakeDeviceAccountsProvider();

  FakeDeviceAccountsProvider(const FakeDeviceAccountsProvider&) = delete;
  FakeDeviceAccountsProvider& operator=(const FakeDeviceAccountsProvider&) =
      delete;

  ~FakeDeviceAccountsProvider() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // DeviceAccountsProvider
  void GetAccessToken(const std::string& account_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAccountsForProfile() const override;
  std::vector<AccountInfo> GetAccountsOnDevice() const override;

  // Methods to configure this fake provider.
  AccountInfo AddAccount(const std::string& gaia, const std::string& email);
  void ClearAccounts();

  // Issues access token responses.
  void IssueAccessTokenForAllRequests();
  void IssueAccessTokenErrorForAllRequests();

 private:
  using AccessTokenRequest = std::pair<std::string, AccessTokenCallback>;

  void FireOnAccountsOnDeviceChanged();

  base::ObserverList<Observer, true> observer_list_;
  std::vector<AccountInfo> accounts_;
  std::vector<AccessTokenRequest> requests_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IOS_FAKE_DEVICE_ACCOUNTS_PROVIDER_H_
