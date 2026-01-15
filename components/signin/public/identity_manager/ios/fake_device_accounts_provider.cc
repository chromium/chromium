// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/ios/fake_device_accounts_provider.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_id.h"

FakeDeviceAccountsProvider::FakeDeviceAccountsProvider() = default;

FakeDeviceAccountsProvider::~FakeDeviceAccountsProvider() = default;

void FakeDeviceAccountsProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeDeviceAccountsProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeDeviceAccountsProvider::GetAccessToken(
    const GaiaId& account_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  requests_.push_back(AccessTokenRequest(account_id, std::move(callback)));
}

std::vector<DeviceAccountsProvider::DeviceAccountInfo>
FakeDeviceAccountsProvider::GetAccountsForProfile() const {
  return accounts_;
}

std::vector<DeviceAccountsProvider::DeviceAccountInfo>
FakeDeviceAccountsProvider::GetAccountsOnDevice() const {
  // TODO(crbug.com/368409110): Add the capability to set accounts-on-device
  // separate from accounts-for-profile.
  return accounts_;
}

DeviceAccountsProvider::DeviceAccountInfo
FakeDeviceAccountsProvider::AddAccount(const GaiaId& gaia,
                                       const std::string& email) {
  DeviceAccountsProvider::DeviceAccountInfo account(gaia, email, "");
  accounts_.push_back(account);
  FireOnAccountsOnDeviceChanged();
  return account;
}

DeviceAccountsProvider::DeviceAccountInfo
FakeDeviceAccountsProvider::UpdateAccount(const GaiaId& gaia,
                                          const std::string& email) {
  for (DeviceAccountInfo& account : accounts_) {
    if (account.GetGaiaId() != gaia) {
      continue;
    }
    account = DeviceAccountInfo(gaia, email, account.GetHostedDomain());
    FireAccountOnDeviceUpdated(account);
    return account;
  }
  NOTREACHED() << "Account with Gaia ID " << gaia << " not found";
}

void FakeDeviceAccountsProvider::ClearAccounts() {
  accounts_.clear();
  FireOnAccountsOnDeviceChanged();
}

void FakeDeviceAccountsProvider::IssueAccessTokenForAllRequests() {
  for (auto& pair : requests_) {
    AccessTokenInfo info{base::StringPrintf("fake_access_token [account=%s]",
                                            pair.first.ToString().c_str()),
                         base::Time::Now() + base::Hours(1)};
    std::move(pair.second).Run(base::ok(std::move(info)));
  }
  requests_.clear();
}

void FakeDeviceAccountsProvider::IssueAccessTokenErrorForAllRequests() {
  for (auto& pair : requests_) {
    std::move(pair.second)
        .Run(base::unexpected(
            GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_SERVER)));
  }
  requests_.clear();
}

void FakeDeviceAccountsProvider::FireOnAccountsOnDeviceChanged() {
  for (auto& observer : observer_list_) {
    observer.OnAccountsOnDeviceChanged();
  }
}

void FakeDeviceAccountsProvider::FireAccountOnDeviceUpdated(
    const DeviceAccountInfo& account) {
  for (auto& observer : observer_list_) {
    observer.OnAccountOnDeviceUpdated(account);
  }
}
