// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_FAKE_PLUS_ADDRESS_SETTING_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_FAKE_PLUS_ADDRESS_SETTING_SERVICE_H_

#include <memory>

#include "components/plus_addresses/settings/plus_address_setting_service.h"

namespace plus_addresses {

// Fake implementation of `PlusAddressSettingService` that does not interact
// with sync.
class FakePlusAddressSettingService : public PlusAddressSettingService {
 public:
  FakePlusAddressSettingService();
  ~FakePlusAddressSettingService() override;
  FakePlusAddressSettingService(const FakePlusAddressSettingService&) = delete;
  FakePlusAddressSettingService& operator=(
      const FakePlusAddressSettingService&) = delete;

  // PlusAddressSettingService:
  bool GetIsPlusAddressesEnabled() const override;
  bool GetHasAcceptedNotice() const override;
  void SetHasAcceptedNotice() override;
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() override;

  void set_has_accepted_notice(bool has_accepted_notice) {
    has_accepted_notice_ = has_accepted_notice;
  }

  void set_is_plus_addresses_enabled(bool is_plus_addresses_enabled) {
    is_plus_addresses_enabled_ = is_plus_addresses_enabled;
  }

 private:
  bool has_accepted_notice_ = true;
  bool is_plus_addresses_enabled_ = true;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_FAKE_PLUS_ADDRESS_SETTING_SERVICE_H_
