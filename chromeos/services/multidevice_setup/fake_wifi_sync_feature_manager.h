// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_WIFI_SYNC_FEATURE_MANAGER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_WIFI_SYNC_FEATURE_MANAGER_H_

#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/wifi_sync_feature_manager.h"

namespace chromeos {

namespace multidevice_setup {

// Test WifiSyncFeatureManager implementation.
class FakeWifiSyncFeatureManager : public WifiSyncFeatureManager {
 public:
  FakeWifiSyncFeatureManager();
  ~FakeWifiSyncFeatureManager() override;

  // WifiSyncFeatureManager:
  void SetIsWifiSyncEnabled(bool enabled) override;
  bool IsWifiSyncEnabled() override;

 private:
  bool is_wifi_sync_enabled_ = false;
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_WIFI_SYNC_FEATURE_MANAGER_H_
