// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_IMPL_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_IMPL_H_

#include <memory>
#include <string_view>

#include "components/plus_addresses/settings/plus_address_setting_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}

namespace plus_addresses {

class PlusAddressSettingSyncBridge;

// Manages settings for `PlusAddressService`. These settings differ from regular
// prefs, since they originate from the user's account and are available beyond
// Chrome.
class PlusAddressSettingServiceImpl : public PlusAddressSettingService {
 public:
  explicit PlusAddressSettingServiceImpl(
      std::unique_ptr<PlusAddressSettingSyncBridge> bridge);
  ~PlusAddressSettingServiceImpl() override;

  // PlusAddressSettingService:
  bool GetIsPlusAddressesEnabled() const override;
  bool GetHasAcceptedNotice() const override;
  void SetHasAcceptedNotice() override;
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() override;

 private:
  // Internal helpers to get the setting value for a given setting name by
  // type. If no setting of the given name exists, the `default_value` is
  // returned.
  // If a setting of the given name exists, but the type doesn't match...
  // - a DCHECK() will fail. This is intended to catch coding errors during
  //   development.
  // - the default value is returned with DCHECKs disabled. This choice was made
  //   to avoid that external factors lead to a crashing state, since settings
  //   are received via the network.
  // No string or int64_t getters exists, since no such settings are synced yet.
  bool GetBoolean(std::string_view name, bool default_value) const;

  const std::unique_ptr<PlusAddressSettingSyncBridge> sync_bridge_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_IMPL_H_
