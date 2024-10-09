// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/ios/fake_device_accounts_provider.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

FakeDeviceAccountsProvider::FakeDeviceAccountsProvider() = default;

FakeDeviceAccountsProvider::~FakeDeviceAccountsProvider() = default;

void FakeDeviceAccountsProvider::GetAccessToken(
    const std::string& account_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  requests_.push_back(AccessTokenRequest(account_id, std::move(callback)));
}

std::vector<DeviceAccountsProvider::AccountInfo>
FakeDeviceAccountsProvider::GetAllAccounts() const {
  return accounts_;
}

DeviceAccountsProvider::AccountInfo FakeDeviceAccountsProvider::AddAccount(
    const std::string& gaia,
    const std::string& email) {
  DeviceAccountsProvider::AccountInfo account;
  account.gaia = gaia;
  account.email = email;
  accounts_.push_back(account);
  return account;
}

void FakeDeviceAccountsProvider::ClearAccounts() {
  accounts_.clear();
}

void FakeDeviceAccountsProvider::IssueAccessTokenForAllRequests() {
  for (auto& pair : requests_) {
    AccessTokenInfo info{base::StringPrintf("fake_access_token [account=%s]",
                                            pair.first.c_str()),
                         base::Time::Now() + base::Hours(1)};
    std::move(pair.second).Run(base::ok(std::move(info)));
  }
  requests_.clear();
}

void FakeDeviceAccountsProvider::IssueAccessTokenErrorForAllRequests() {
  for (auto& pair : requests_) {
    std::move(pair.second)
        .Run(base::unexpected(kAuthenticationErrorCategoryAuthorizationErrors));
  }
  requests_.clear();
}
