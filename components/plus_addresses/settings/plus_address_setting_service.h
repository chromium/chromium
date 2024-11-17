// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace plus_addresses {

class PlusAddressSettingService : public KeyedService {
 public:
  PlusAddressSettingService() = default;
  ~PlusAddressSettingService() override = default;
  PlusAddressSettingService(const PlusAddressSettingService&) = delete;
  PlusAddressSettingService& operator=(const PlusAddressSettingService&) =
      delete;

  // Getters for the settings. If the client isn't aware of the value of a
  // setting, a setting-specific default value is returned.

  // Defaults to true.
  virtual bool GetIsPlusAddressesEnabled() const = 0;
  // Whether the user went through the onboarding flow. Defaults to false.
  virtual bool GetHasAcceptedNotice() const = 0;

  // Setters for the settings writable from Chrome.
  // Sets the state that the user has accepted the notice to true.
  virtual void SetHasAcceptedNotice() = 0;

  // Returns a controller delegate for the `sync_bridge_` owned by this service.
  virtual std::unique_ptr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() = 0;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
