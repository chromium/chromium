// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_BLE_CONSTANTS_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_BLE_CONSTANTS_H_

namespace ash::secure_channel {

// The maximum number of devices to which to advertise concurrently. If more
// than this number of devices are registered, other advertisements must be
// stopped before new ones can be added.
//
// Note that this upper limit on concurrent advertisements is imposed due to a
// hardware limit of advertisements (many devices have <10 total advertisement
// slots).
constexpr const size_t kMaxConcurrentAdvertisements = 2;

// The service UUID used for BLE advertisements.
constexpr const char kAdvertisingServiceUuid[] =
    "0000fe50-0000-1000-8000-00805f9b34fb";
const std::vector<uint8_t> kAdvertisingServiceUuidAsBytes = {0x50, 0xfe};

// The GATT server UUID used for uWeave.
constexpr const char kGattServerUuid[] = "b3b7e28e-a000-3e17-bd86-6e97b9e28c11";

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_BLE_CONSTANTS_H_
