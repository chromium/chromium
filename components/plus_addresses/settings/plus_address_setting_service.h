// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {
class ModelTypeControllerDelegate;
}

namespace plus_addresses {

class PlusAddressSettingSyncBridge;

// Manages settings for `PlusAddressService`. These settings differ from regular
// prefs, since they originate from the user's account and are available beyond
// Chrome.
// TODO(b/342089839): Add a public API.
class PlusAddressSettingService : public KeyedService {
 public:
  explicit PlusAddressSettingService(
      syncer::OnceModelTypeStoreFactory store_factory);
  ~PlusAddressSettingService() override;

  // Returns a controller delegate for the `sync_bridge_` owned by this service.
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate();

 private:
  std::unique_ptr<PlusAddressSettingSyncBridge> sync_bridge_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
