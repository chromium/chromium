// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

DeviceAccountsProvider::AccountInfo::AccountInfo(GaiaId gaia,
                                                 std::string email,
                                                 std::string hosted_domain,
                                                 bool has_persistent_auth_error)
    : gaia_(std::move(gaia)),
      email_(std::move(email)),
      hosted_domain_(std::move(hosted_domain)),
      has_persistent_auth_error_(has_persistent_auth_error) {
  CHECK(!gaia_.empty());
  CHECK(!email_.empty());
}

DeviceAccountsProvider::AccountInfo::AccountInfo(const AccountInfo& other) =
    default;

DeviceAccountsProvider::AccountInfo&
DeviceAccountsProvider::AccountInfo::operator=(const AccountInfo& other) =
    default;

DeviceAccountsProvider::AccountInfo::AccountInfo(AccountInfo&& other) = default;

DeviceAccountsProvider::AccountInfo&
DeviceAccountsProvider::AccountInfo::operator=(AccountInfo&& other) = default;

DeviceAccountsProvider::AccountInfo::~AccountInfo() = default;

const GaiaId& DeviceAccountsProvider::AccountInfo::GetGaiaId() const {
  return gaia_;
}

const std::string& DeviceAccountsProvider::AccountInfo::GetEmail() const {
  return email_;
}

const std::string& DeviceAccountsProvider::AccountInfo::GetHostedDomain()
    const {
  return hosted_domain_;
}

bool DeviceAccountsProvider::AccountInfo::HasPersistentAuthError() const {
  return has_persistent_auth_error_;
}

std::vector<DeviceAccountsProvider::AccountInfo>
DeviceAccountsProvider::GetAccountsForProfile() const {
  return std::vector<DeviceAccountsProvider::AccountInfo>();
}

std::vector<DeviceAccountsProvider::AccountInfo>
DeviceAccountsProvider::GetAccountsOnDevice() const {
  return std::vector<DeviceAccountsProvider::AccountInfo>();
}

void DeviceAccountsProvider::GetAccessToken(const GaiaId& gaia_id,
                                            const std::string& client_id,
                                            const std::set<std::string>& scopes,
                                            AccessTokenCallback callback) {}
