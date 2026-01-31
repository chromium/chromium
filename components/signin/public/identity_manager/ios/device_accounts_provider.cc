// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

DeviceAccountsProvider::DeviceAccountInfo::DeviceAccountInfo(
    GaiaId gaia,
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

DeviceAccountsProvider::DeviceAccountInfo::DeviceAccountInfo(
    const DeviceAccountInfo& other) = default;

DeviceAccountsProvider::DeviceAccountInfo&
DeviceAccountsProvider::DeviceAccountInfo::operator=(
    const DeviceAccountInfo& other) = default;

DeviceAccountsProvider::DeviceAccountInfo::DeviceAccountInfo(
    DeviceAccountInfo&& other) = default;

DeviceAccountsProvider::DeviceAccountInfo&
DeviceAccountsProvider::DeviceAccountInfo::operator=(
    DeviceAccountInfo&& other) = default;

DeviceAccountsProvider::DeviceAccountInfo::~DeviceAccountInfo() = default;

const GaiaId& DeviceAccountsProvider::DeviceAccountInfo::GetGaiaId() const {
  return gaia_;
}

const std::string& DeviceAccountsProvider::DeviceAccountInfo::GetEmail() const {
  return email_;
}

const std::string& DeviceAccountsProvider::DeviceAccountInfo::GetHostedDomain()
    const {
  return hosted_domain_;
}

bool DeviceAccountsProvider::DeviceAccountInfo::HasPersistentAuthError() const {
  return has_persistent_auth_error_;
}

std::vector<DeviceAccountsProvider::DeviceAccountInfo>
DeviceAccountsProvider::GetAccountsForProfile() const {
  return std::vector<DeviceAccountsProvider::DeviceAccountInfo>();
}

std::vector<DeviceAccountsProvider::DeviceAccountInfo>
DeviceAccountsProvider::GetAccountsOnDevice() const {
  return std::vector<DeviceAccountsProvider::DeviceAccountInfo>();
}

void DeviceAccountsProvider::GetAccessToken(const GaiaId& gaia_id,
                                            const std::string& client_id,
                                            const std::set<std::string>& scopes,
                                            AccessTokenCallback callback) {}
