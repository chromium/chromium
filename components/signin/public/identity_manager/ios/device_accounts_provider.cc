// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

std::vector<DeviceAccountsProvider::AccountInfo>
DeviceAccountsProvider::GetAllAccounts() const {
  return std::vector<DeviceAccountsProvider::AccountInfo>();
}

void DeviceAccountsProvider::GetAccessToken(const std::string& gaia_id,
                                            const std::string& client_id,
                                            const std::set<std::string>& scopes,
                                            AccessTokenCallback callback) {}
