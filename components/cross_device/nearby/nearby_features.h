// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CROSS_DEVICE_NEARBY_NEARBY_FEATURES_H_
#define COMPONENTS_CROSS_DEVICE_NEARBY_NEARBY_FEATURES_H_

#include "base/feature_list.h"

namespace features {

BASE_DECLARE_FEATURE(kEnableNearbyBleV2);
BASE_DECLARE_FEATURE(kEnableNearbyBleV2ExtendedAdvertising);
BASE_DECLARE_FEATURE(kEnableNearbyBluetoothClassicAdvertising);
BASE_DECLARE_FEATURE(kEnableNearbyBluetoothClassicScanning);
bool IsNearbyBleV2Enabled();
bool IsNearbyBleV2ExtendedAdvertisingEnabled();
bool IsNearbyBluetoothClassicAdvertisingEnabled();
bool IsNearbyBluetoothClassicScanningEnabled();
BASE_DECLARE_FEATURE(kNearbySharingWebRtc);
BASE_DECLARE_FEATURE(kNearbySharingWifiLan);

}  // namespace features

#endif  // COMPONENTS_CROSS_DEVICE_NEARBY_NEARBY_FEATURES_H_
