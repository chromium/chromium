// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_

#include <memory>
#include <string_view>

#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}

namespace autofill {

class AccountSettingSyncBridge;

// Manages settings stored in your Google account. These settings differ from
// regular prefs, since they originate from the user's account and are available
// beyond Chrome.
class AccountSettingService : public KeyedService {
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
  std::unique_ptr<AccountSettingSyncBridge> sync_bridge_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
