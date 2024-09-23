// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"

#include <memory>

namespace plus_addresses {

FakePlusAddressSettingService::FakePlusAddressSettingService() = default;

FakePlusAddressSettingService::~FakePlusAddressSettingService() = default;

bool FakePlusAddressSettingService::GetIsPlusAddressesEnabled() const {
  return is_plus_addresses_enabled_;
}

bool FakePlusAddressSettingService::GetHasAcceptedNotice() const {
  return has_accepted_notice_;
}

void FakePlusAddressSettingService::SetHasAcceptedNotice() {
  set_has_accepted_notice(true);
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
FakePlusAddressSettingService::GetSyncControllerDelegate() {
  return nullptr;
}

}  // namespace plus_addresses
