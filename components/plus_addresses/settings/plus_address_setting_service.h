// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_

#include <memory>
#include <string_view>

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
class PlusAddressSettingService : public KeyedService {
 public:
  explicit PlusAddressSettingService(
      syncer::OnceModelTypeStoreFactory store_factory);
  ~PlusAddressSettingService() override;

  // Getters for the settings. If the client isn't aware of the value of a
  // setting yet (because it's still being downloaded by sync), the default
  // value (false, "" or 0) is returned.

  bool GetIsPlusAddressesEnabled() const;
  // Whether the user went through the onboarding flow.
  bool GetHasAcceptedNotice() const;
  // Whether the signed-in user is enrolled in the beta rollout of the feature.
  // This is different from `!GetIsPlusAddressesEnabled()`, since for users that
  // have not opted in, no UI will be shown.
  // This is necessary, since group targeted rollouts are per installation.
  bool GetIsOptedInToDogfood() const;

  // Returns a controller delegate for the `sync_bridge_` owned by this service.
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate();

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

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
