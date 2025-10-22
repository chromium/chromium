// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_

#include <memory>
#include <string_view>

#include "base/scoped_observation.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}

namespace autofill {

// Manages settings stored in your Google account. These settings differ from
// regular prefs, since they originate from the user's account and are available
// beyond Chrome.
class AccountSettingService : public KeyedService,
                              public AccountSettingSyncBridge::Observer {
 public:
  explicit AccountSettingService(
      std::unique_ptr<AccountSettingSyncBridge> bridge);
  ~AccountSettingService() override;

  // Getter to check whether the user agreed to share data from Wallet to other
  // Google services, including Chrome, in their Google account settings.
  bool IsWalletPrivacyContextualSurfacingEnabled() const;

  // Returns a controller delegate for the `sync_bridge_` owned by this service.
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate();

 private:
  // AccountSettingSyncBridge::Observer:
  void OnDataLoadedFromDisk() override;

  std::unique_ptr<AccountSettingSyncBridge> sync_bridge_;

  base::ScopedObservation<AccountSettingSyncBridge,
                          AccountSettingSyncBridge::Observer>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
