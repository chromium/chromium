// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_FEATURE_MANAGER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_FEATURE_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"

namespace chromeos {

namespace multidevice_setup {

// Manager for setting and receiving the Wifi Sync Host enabled/disabled state.
// This class is considered the source of truth for the current state of Wifi
// Sync Host.
class WifiSyncFeatureManager {
 public:
  virtual ~WifiSyncFeatureManager() = default;
  WifiSyncFeatureManager(const WifiSyncFeatureManager&) = delete;
  WifiSyncFeatureManager& operator=(const WifiSyncFeatureManager&) = delete;

  // Attempts to enable/disable Wifi Sync on the backend for the host
  // device that is synced at the time SetIsWifiSyncEnabled  is called.
  virtual void SetIsWifiSyncEnabled(bool enabled) = 0;

  // Returns whether Wifi Sync is enabled/disabled.
  virtual bool IsWifiSyncEnabled() = 0;

 protected:
  WifiSyncFeatureManager() = default;
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_FEATURE_MANAGER_H_