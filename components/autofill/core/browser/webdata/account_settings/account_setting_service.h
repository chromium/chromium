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
  // Internals helpers to get the setting value for a given setting name by
  // type. If no setting of the given name exists, `default_value` is returned.
  //
  // If a setting of the given name exists, but the type doesn't match...
  // - a DCHECK() will fail. This is intended to catch coding errors during
  //   development.
  // - `default_value` is returned with DCHECKs disabled. This choice was made
  //   to avoid that external factors lead to a crashing state, since settings
  //   are received via the network.
  bool GetBoolean(std::string_view name, bool default_value) const;

  std::unique_ptr<AccountSettingSyncBridge> sync_bridge_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
