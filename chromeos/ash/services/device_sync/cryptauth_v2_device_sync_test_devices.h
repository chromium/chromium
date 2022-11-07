// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_SYNC_TEST_DEVICES_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_SYNC_TEST_DEVICES_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"

namespace cryptauthv2 {
class DeviceMetadataPacket;
}  // namespace cryptauthv2

namespace ash {

namespace device_sync {

class CryptAuthDevice;

// Provides data for three hypothetical devices:
//   - The local device, which is the Chromebook making the DeviceSync calls.
//     Its data is consistent with GetClientAppMetadataForTest(), namely the
//     Instance ID and device model, which is used as the PII-free device name.
//   - A remote device that needs the group private key. Specifically, the
//     need_group_private_key field of its DeviceMetadataPacket is true.
//   - A remote device that has the group private key. Specifically, the
//     need_group_private_key field of its DeviceMetadataPacket is false.
//
// It is assumed that kGroupPublicKey is the correct group public key. Notably,
// the three test devices' DeviceMetadataPackets use kGroupPublic for metadata
// encryption.

// A hypothetical group public key.
extern const char kGroupPublicKey[];

// The first 8 bytes of the SHA-256 group-public-key hash, converted into a
// 64-bit signed integer in little-endian order. This is needed when sharing
// the group private key.
extern const int64_t kGroupPublicKeyHash;

// Bluetooth address for the local device.
extern const char kDefaultLocalDeviceBluetoothAddress[];

// Three test devices: The local device, a remote device that needs the group
// private key, and a remote device that has the group private key.
const CryptAuthDevice& GetLocalDeviceForTest();
const CryptAuthDevice& GetRemoteDeviceNeedsGroupPrivateKeyForTest();
const CryptAuthDevice& GetRemoteDeviceHasGroupPrivateKeyForTest();
const CryptAuthDevice& GetTestDeviceWithId(const std::string& id);
const std::vector<CryptAuthDevice>& GetAllTestDevices();
const std::vector<CryptAuthDevice>& GetAllTestDevicesWithoutRemoteMetadata();

const base::flat_set<std::string>& GetAllTestDeviceIds();
const base::flat_set<std::string>& GetAllTestDeviceIdsThatNeedGroupPrivateKey();

// DeviceMetadataPackets corresponding to the test devices above. Assumes
// metadata is encrypted with kGroupPublicKey using MakeFakeEncryptedString().
cryptauthv2::DeviceMetadataPacket ConvertTestDeviceToMetadataPacket(
    const CryptAuthDevice& device,
    const std::string& group_public_key,
    bool need_group_private_key);
const cryptauthv2::DeviceMetadataPacket& GetLocalDeviceMetadataPacketForTest(
    CryptAuthDevice& device);
const cryptauthv2::DeviceMetadataPacket& GetLocalDeviceMetadataPacketForTest();
const cryptauthv2::DeviceMetadataPacket&
GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest();
const cryptauthv2::DeviceMetadataPacket&
GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest();
const std::vector<cryptauthv2::DeviceMetadataPacket>&
GetAllTestDeviceMetadataPackets();

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_SYNC_TEST_DEVICES_H_
