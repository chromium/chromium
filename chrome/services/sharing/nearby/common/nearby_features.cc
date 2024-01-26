// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/common/nearby_features.h"

#include "base/feature_list.h"

namespace features {

// Enables the use of Extended Advertising from within the implementation of BLE
// V2, for incremental testing purposes. Assumes that the caller will also check
// if the hardware supports Extended Advertising.
BASE_FEATURE(kEnableNearbyBleV2ExtendedAdvertising,
             "EnableNearbyBleV2ExtendedAdvertising",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of Bluetooth Classic advertising from within the
// implementation of Nearby Connections, for incremental testing purposes.
BASE_FEATURE(kEnableNearbyBluetoothClassicAdvertising,
             "EnableNearbyBluetoothClassicAdvertising",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNearbyBleV2ExtendedAdvertisingEnabled() {
  return base::FeatureList::IsEnabled(kEnableNearbyBleV2ExtendedAdvertising);
}

bool IsNearbyBluetoothClassicAdvertisingEnabled() {
  return base::FeatureList::IsEnabled(kEnableNearbyBluetoothClassicAdvertising);
}

}  // namespace features
