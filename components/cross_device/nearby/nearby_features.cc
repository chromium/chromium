// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cross_device/nearby/nearby_features.h"

#include "base/feature_list.h"

namespace features {

// Enables the use of BleV2. This flag is CrOS owned, and overrides the value of
// the flag "kEnableBleV2" owned by Nearby Connections.
BASE_FEATURE(kEnableNearbyBleV2, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of Extended Advertising from within the implementation of BLE
// V2, for incremental testing purposes. Assumes that the caller will also check
// if the hardware supports Extended Advertising.
BASE_FEATURE(kEnableNearbyBleV2ExtendedAdvertising,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of Bluetooth Classic advertising from within the
// implementation of Nearby Connections, for incremental testing purposes.
BASE_FEATURE(kEnableNearbyBluetoothClassicAdvertising,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of Bluetooth Classic advertising from within the
// implementation of Nearby Connections, for incremental testing purposes.
BASE_FEATURE(kEnableNearbyBluetoothClassicScanning,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables use of WebRTC in Nearby Connections for Nearby Share.
BASE_FEATURE(kNearbySharingWebRtc, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables use of WifiDirect in Nearby Share.
BASE_FEATURE(kNearbySharingWifiDirect, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables use of WifiLan in Nearby Share.
BASE_FEATURE(kNearbySharingWifiLan, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNearbyBleV2Enabled() {
  return base::FeatureList::IsEnabled(kEnableNearbyBleV2);
}

bool IsNearbyBleV2ExtendedAdvertisingEnabled() {
  return base::FeatureList::IsEnabled(kEnableNearbyBleV2ExtendedAdvertising);
}

bool IsNearbyBluetoothClassicAdvertisingEnabled() {
  return base::FeatureList::IsEnabled(kEnableNearbyBluetoothClassicAdvertising);
}

bool IsNearbyBluetoothClassicScanningEnabled() {
  return base::FeatureList::IsEnabled(kEnableNearbyBluetoothClassicScanning);
}

}  // namespace features
