// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_device_sync_test_devices.h"

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/fake_ecies_encryption.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"

namespace ash {

namespace device_sync {

const char kGroupPublicKey[] = "group_key";
const int64_t kGroupPublicKeyHash = 0xf3666041a2db06e4;
const char kDefaultLocalDeviceBluetoothAddress[] = "01:23:45:67:89:AB";

const CryptAuthDevice& GetLocalDeviceForTest() {
  static const base::NoDestructor<CryptAuthDevice> device([] {
    // Note: The local device's (Instance) ID and PII-free device name are not
    // explicitly defined here, instead using values from
    // GetClientAppMetadataForTest().
    const char kLocalDeviceDeviceName[] = "local_device: device_name";
    const char kLocalDeviceUserPublicKey[] = "local_device: user_key";
    const char kLocalDeviceDeviceSyncBetterTogetherPublicKey[] =
        "local_device: dsbt_key";

    cryptauthv2::BetterTogetherDeviceMetadata bt_metadata;
    bt_metadata.set_public_key(kLocalDeviceUserPublicKey);
    bt_metadata.set_no_pii_device_name(
        cryptauthv2::GetClientAppMetadataForTest().device_model());
    bt_metadata.set_bluetooth_public_address(
        kDefaultLocalDeviceBluetoothAddress);

    return CryptAuthDevice(
        cryptauthv2::GetClientAppMetadataForTest().instance_id(),
        kLocalDeviceDeviceName, kLocalDeviceDeviceSyncBetterTogetherPublicKey,
        base::Time::FromMillisecondsSinceUnixEpoch(100), bt_metadata,
        {
            {multidevice::SoftwareFeature::kBetterTogetherHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kBetterTogetherClient,
             multidevice::SoftwareFeatureState::kEnabled},
            {multidevice::SoftwareFeature::kSmartLockHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kSmartLockClient,
             multidevice::SoftwareFeatureState::kEnabled},
            {multidevice::SoftwareFeature::kInstantTetheringHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kInstantTetheringClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kMessagesForWebHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kMessagesForWebClient,
             multidevice::SoftwareFeatureState::kSupported},
            {multidevice::SoftwareFeature::kPhoneHubHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kWifiSyncHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kWifiSyncClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kEcheHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kEcheClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubCameraRollClient,
             multidevice::SoftwareFeatureState::kNotSupported},
        });
  }());
  return *device;
}

const CryptAuthDevice& GetRemoteDeviceNeedsGroupPrivateKeyForTest() {
  static const base::NoDestructor<CryptAuthDevice> device([] {
    const char kRemoteDeviceNeedsGroupPrivateKeyId[] =
        "remote_device_needs_group_private_key: device_id";
    const char kRemoteDeviceNeedsGroupPrivateKeyDeviceName[] =
        "remote_device_needs_group_private_key: device_name";
    const char kRemoteDeviceNeedsGroupPrivateKeyNoPiiDeviceName[] =
        "remote_device_needs_group_private_key: no_pii_device_name";
    const char kRemoteDeviceNeedsGroupPrivateKeyUserPublicKey[] =
        "remote_device_needs_group_private_key: user_key";
    const char
        kRemoteDeviceNeedsGroupPrivateKeyDeviceSyncBetterTogetherPublicKey[] =
            "remote_device_needs_group_private_key: dsbt_key";

    cryptauthv2::BetterTogetherDeviceMetadata bt_metadata;
    bt_metadata.set_public_key(kRemoteDeviceNeedsGroupPrivateKeyUserPublicKey);
    bt_metadata.set_no_pii_device_name(
        kRemoteDeviceNeedsGroupPrivateKeyNoPiiDeviceName);

    return CryptAuthDevice(
        kRemoteDeviceNeedsGroupPrivateKeyId,
        kRemoteDeviceNeedsGroupPrivateKeyDeviceName,
        kRemoteDeviceNeedsGroupPrivateKeyDeviceSyncBetterTogetherPublicKey,
        base::Time::FromMillisecondsSinceUnixEpoch(200), bt_metadata,
        {
            {multidevice::SoftwareFeature::kBetterTogetherHost,
             multidevice::SoftwareFeatureState::kEnabled},
            {multidevice::SoftwareFeature::kBetterTogetherClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kSmartLockHost,
             multidevice::SoftwareFeatureState::kEnabled},
            {multidevice::SoftwareFeature::kSmartLockClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kInstantTetheringHost,
             multidevice::SoftwareFeatureState::kSupported},
            {multidevice::SoftwareFeature::kInstantTetheringClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kMessagesForWebHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kMessagesForWebClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kWifiSyncHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kWifiSyncClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kEcheHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kEcheClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubCameraRollClient,
             multidevice::SoftwareFeatureState::kNotSupported},
        });
  }());
  return *device;
}

const CryptAuthDevice& GetRemoteDeviceHasGroupPrivateKeyForTest() {
  static const base::NoDestructor<CryptAuthDevice> device([] {
    const char kRemoteDeviceHasGroupPrivateKeyId[] =
        "remote_device_has_group_private_key: device_id";
    const char kRemoteDeviceHasGroupPrivateKeyDeviceName[] =
        "remote_device_has_group_private_key: device_name";
    const char kRemoteDeviceHasGroupPrivateKeyNoPiiDeviceName[] =
        "remote_device_has_group_private_key: no_pii_device_name";
    const char kRemoteDeviceHasGroupPrivateKeyUserPublicKey[] =
        "remote_device_has_group_private_key: user_key";
    const char
        kRemoteDeviceHasGroupPrivateKeyDeviceSyncBetterTogetherPublicKey[] =
            "remote_device_has_group_private_key: dsbt_key";

    cryptauthv2::BetterTogetherDeviceMetadata bt_metadata;
    bt_metadata.set_public_key(kRemoteDeviceHasGroupPrivateKeyUserPublicKey);
    bt_metadata.set_no_pii_device_name(
        kRemoteDeviceHasGroupPrivateKeyNoPiiDeviceName);

    return CryptAuthDevice(
        kRemoteDeviceHasGroupPrivateKeyId,
        kRemoteDeviceHasGroupPrivateKeyDeviceName,
        kRemoteDeviceHasGroupPrivateKeyDeviceSyncBetterTogetherPublicKey,
        base::Time::FromMillisecondsSinceUnixEpoch(300), bt_metadata,
        {
            {multidevice::SoftwareFeature::kBetterTogetherHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kBetterTogetherClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kSmartLockHost,
             multidevice::SoftwareFeatureState::kSupported},
            {multidevice::SoftwareFeature::kSmartLockClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kInstantTetheringHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kInstantTetheringClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kMessagesForWebHost,
             multidevice::SoftwareFeatureState::kEnabled},
            {multidevice::SoftwareFeature::kMessagesForWebClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kWifiSyncHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kWifiSyncClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kEcheHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kEcheClient,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
             multidevice::SoftwareFeatureState::kNotSupported},
            {multidevice::SoftwareFeature::kPhoneHubCameraRollClient,
             multidevice::SoftwareFeatureState::kNotSupported},
        });
  }());
  return *device;
}

const CryptAuthDevice& GetTestDeviceWithId(const std::string& id) {
  if (id == GetLocalDeviceForTest().instance_id())
    return GetLocalDeviceForTest();

  if (id == GetRemoteDeviceNeedsGroupPrivateKeyForTest().instance_id())
    return GetRemoteDeviceNeedsGroupPrivateKeyForTest();

  DCHECK_EQ(id, GetRemoteDeviceHasGroupPrivateKeyForTest().instance_id());
  return GetRemoteDeviceHasGroupPrivateKeyForTest();
}

const std::vector<CryptAuthDevice>& GetAllTestDevices() {
  static const base::NoDestructor<std::vector<CryptAuthDevice>> all_devices([] {
    return std::vector<CryptAuthDevice>{
        GetLocalDeviceForTest(), GetRemoteDeviceNeedsGroupPrivateKeyForTest(),
        GetRemoteDeviceHasGroupPrivateKeyForTest()};
  }());
  return *all_devices;
}

const std::vector<CryptAuthDevice>& GetAllTestDevicesWithoutRemoteMetadata() {
  static const base::NoDestructor<std::vector<CryptAuthDevice>>
      all_devices_without_metadata([] {
        std::vector<CryptAuthDevice> devices_without_metadata =
            GetAllTestDevices();
        for (CryptAuthDevice& device : devices_without_metadata) {
          // The local device always has its BetterTogetherDeviceMetadata.
          if (device.instance_id() == GetLocalDeviceForTest().instance_id())
            continue;

          device.better_together_device_metadata.reset();
        }

        return devices_without_metadata;
      }());
  return *all_devices_without_metadata;
}

const base::flat_set<std::string>& GetAllTestDeviceIds() {
  static const base::NoDestructor<base::flat_set<std::string>> all_device_ids(
      [] {
        return base::flat_set<std::string>{
            GetLocalDeviceForTest().instance_id(),
            GetRemoteDeviceNeedsGroupPrivateKeyForTest().instance_id(),
            GetRemoteDeviceHasGroupPrivateKeyForTest().instance_id()};
      }());
  return *all_device_ids;
}

const base::flat_set<std::string>&
GetAllTestDeviceIdsThatNeedGroupPrivateKey() {
  static const base::NoDestructor<base::flat_set<std::string>> device_ids([] {
    base::flat_set<std::string> device_ids;
    for (const cryptauthv2::DeviceMetadataPacket& metadata :
         GetAllTestDeviceMetadataPackets()) {
      if (metadata.need_group_private_key())
        device_ids.insert(metadata.device_id());
    }
    return device_ids;
  }());
  return *device_ids;
}

cryptauthv2::DeviceMetadataPacket ConvertTestDeviceToMetadataPacket(
    const CryptAuthDevice& device,
    const std::string& group_public_key,
    bool need_group_private_key) {
  cryptauthv2::DeviceMetadataPacket packet;
  packet.set_device_id(device.instance_id());
  packet.set_need_group_private_key(need_group_private_key);
  packet.set_device_public_key(device.device_better_together_public_key);
  packet.set_device_name(device.device_name);
  if (device.better_together_device_metadata) {
    packet.set_encrypted_metadata(MakeFakeEncryptedString(
        device.better_together_device_metadata->SerializeAsString(),
        group_public_key));
  }
  return packet;
}

const cryptauthv2::DeviceMetadataPacket& GetLocalDeviceMetadataPacketForTest(
    CryptAuthDevice& device) {
  static const base::NoDestructor<cryptauthv2::DeviceMetadataPacket> packet(
      [device] {
        return ConvertTestDeviceToMetadataPacket(
            device, kGroupPublicKey, true /* need_group_private_key */);
      }());
  return *packet;
}

const cryptauthv2::DeviceMetadataPacket& GetLocalDeviceMetadataPacketForTest() {
  static const base::NoDestructor<cryptauthv2::DeviceMetadataPacket> device([] {
    return ConvertTestDeviceToMetadataPacket(GetLocalDeviceForTest(),
                                             kGroupPublicKey,
                                             true /* need_group_private_key */);
  }());
  return *device;
}

const cryptauthv2::DeviceMetadataPacket&
GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest() {
  static const base::NoDestructor<cryptauthv2::DeviceMetadataPacket> device([] {
    return ConvertTestDeviceToMetadataPacket(
        GetRemoteDeviceNeedsGroupPrivateKeyForTest(), kGroupPublicKey,
        true /* need_group_private_key */);
  }());
  return *device;
}

const cryptauthv2::DeviceMetadataPacket&
GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest() {
  static const base::NoDestructor<cryptauthv2::DeviceMetadataPacket> device([] {
    return ConvertTestDeviceToMetadataPacket(
        GetRemoteDeviceHasGroupPrivateKeyForTest(), kGroupPublicKey,
        false /* need_group_private_key */);
  }());
  return *device;
}

const std::vector<cryptauthv2::DeviceMetadataPacket>&
GetAllTestDeviceMetadataPackets() {
  static const base::NoDestructor<
      std::vector<cryptauthv2::DeviceMetadataPacket>>
      all_device_metadata_packets([] {
        return std::vector<cryptauthv2::DeviceMetadataPacket>{
            GetLocalDeviceMetadataPacketForTest(),
            GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest(),
            GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest()};
      }());
  return *all_device_metadata_packets;
}

}  // namespace device_sync

}  // namespace ash
