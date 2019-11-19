// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/ios/fake_device_accounts_provider.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeDeviceAccountsProvider::FakeDeviceAccountsProvider() {}

FakeDeviceAccountsProvider::~FakeDeviceAccountsProvider() {}

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
    NSString* access_token = [NSString
        stringWithFormat:@"fake_access_token [account=%s]", pair.first.c_str()];
    NSDate* one_hour_from_now = [NSDate dateWithTimeIntervalSinceNow:3600];
    std::move(pair.second).Run(access_token, one_hour_from_now, nil);
  }
  requests_.clear();
}

void FakeDeviceAccountsProvider::IssueAccessTokenErrorForAllRequests() {
  for (auto& pair : requests_) {
    NSError* error = [[NSError alloc] initWithDomain:@"fake_access_token_error"
                                                code:-1
                                            userInfo:nil];
    std::move(pair.second).Run(nil, nil, error);
  }
  requests_.clear();
}

AuthenticationErrorCategory
FakeDeviceAccountsProvider::GetAuthenticationErrorCategory(
    const std::string& gaia_id,
    NSError* error) const {
  DCHECK(error);
  return kAuthenticationErrorCategoryAuthorizationErrors;
}
