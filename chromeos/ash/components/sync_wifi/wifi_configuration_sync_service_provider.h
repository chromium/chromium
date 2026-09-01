// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_PROVIDER_H_

class AccountId;

namespace ash {

namespace sync_wifi {
class WifiConfigurationSyncService;
}  // namespace sync_wifi

// Provides the sync_wifi::WifiConfigurationSyncService associated with a user
// to ChromeOS callers without forcing them to depend on //chrome/browser/sync's
// Profile-keyed factory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// wifi_configuration_sync_service_provider_impl.h).
class WifiConfigurationSyncServiceProvider {
 public:
  WifiConfigurationSyncServiceProvider();
  WifiConfigurationSyncServiceProvider(
      const WifiConfigurationSyncServiceProvider&) = delete;
  WifiConfigurationSyncServiceProvider& operator=(
      const WifiConfigurationSyncServiceProvider&) = delete;
  virtual ~WifiConfigurationSyncServiceProvider();

  // Returns the process-wide singleton.
  static WifiConfigurationSyncServiceProvider& Get();

  // Returns the WifiConfigurationSyncService associated with `account_id`, or
  // nullptr if none exists (the service is not created on demand). The returned
  // pointer is owned by the BrowserContext-keyed service infrastructure;
  // callers must not delete it.
  virtual sync_wifi::WifiConfigurationSyncService* Find(
      const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_PROVIDER_H_
