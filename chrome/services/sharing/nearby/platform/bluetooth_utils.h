// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_UTILS_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_UTILS_H_

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/nearby/src/internal/platform/uuid.h"

namespace nearby::chrome {

nearby::Uuid BluetoothUuidToNearbyUuid(
    const device::BluetoothUUID& bluetooth_service_uuid);

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_UTILS_H_
