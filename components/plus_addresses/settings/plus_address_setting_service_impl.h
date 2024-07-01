// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_IMPL_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_IMPL_H_

#include <memory>
#include <string_view>

#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {
class ModelTypeControllerDelegate;
}

namespace plus_addresses {

class PlusAddressSettingSyncBridge;

// Manages settings for `PlusAddressService`. These settings differ from regular
// prefs, since they originate from the user's account and are available beyond
// Chrome.
class PlusAddressSettingServiceImpl : public PlusAddressSettingService {
 public:
  explicit PlusAddressSettingServiceImpl(
      syncer::OnceModelTypeStoreFactory store_factory);
  ~PlusAddressSettingServiceImpl() override;

  // PlusAddressSettingService:
  bool GetIsPlusAddressesEnabled() const override;
  bool GetHasAcceptedNotice() const override;
  bool GetIsOptedInToDogfood() const override;
  void SetHasAcceptedNotice() override;
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate() override;

 private:
  // Internals helpers to get the setting value for a given setting name by
  // type. If a setting of the given name exists, but the type doesn't match, a
  // CHECK() will fail. If no setting of the given name exists, the default
  // value is returned.
  // No string or int64_t getters exists, since no such settings are synced yet.
  bool GetBoolean(std::string_view name) const;

  std::unique_ptr<PlusAddressSettingSyncBridge> sync_bridge_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_IMPL_H_
