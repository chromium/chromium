// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_wifi_sync_feature_manager.h"

namespace chromeos {

namespace multidevice_setup {

FakeWifiSyncFeatureManager::FakeWifiSyncFeatureManager()
    : WifiSyncFeatureManager() {}

FakeWifiSyncFeatureManager::~FakeWifiSyncFeatureManager() = default;

void FakeWifiSyncFeatureManager::SetIsWifiSyncEnabled(bool enabled) {
  is_wifi_sync_enabled_ = enabled;
}

bool FakeWifiSyncFeatureManager::IsWifiSyncEnabled() {
  return is_wifi_sync_enabled_;
}

}  // namespace multidevice_setup

}  // namespace chromeos
