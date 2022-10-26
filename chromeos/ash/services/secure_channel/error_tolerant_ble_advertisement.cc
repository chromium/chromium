// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/error_tolerant_ble_advertisement.h"

namespace ash::secure_channel {

ErrorTolerantBleAdvertisement::ErrorTolerantBleAdvertisement(
    const DeviceIdPair& device_id_pair)
    : device_id_pair_(device_id_pair) {}

ErrorTolerantBleAdvertisement::~ErrorTolerantBleAdvertisement() = default;

}  // namespace ash::secure_channel
