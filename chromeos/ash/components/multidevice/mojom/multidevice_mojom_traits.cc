// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/mojom/multidevice_mojom_traits.h"

#include "base/notreached.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

const std::string& StructTraits<ash::multidevice::mojom::BeaconSeedDataView,
                                ash::multidevice::BeaconSeed>::
    data(const ash::multidevice::BeaconSeed& beacon_seed) {
  return beacon_seed.data();
}

base::Time StructTraits<ash::multidevice::mojom::BeaconSeedDataView,
                        ash::multidevice::BeaconSeed>::
    start_time(const ash::multidevice::BeaconSeed& beacon_seed) {
  return beacon_seed.start_time();
}

base::Time StructTraits<ash::multidevice::mojom::BeaconSeedDataView,
                        ash::multidevice::BeaconSeed>::
    end_time(const ash::multidevice::BeaconSeed& beacon_seed) {
  return beacon_seed.end_time();
}

bool StructTraits<ash::multidevice::mojom::BeaconSeedDataView,
                  ash::multidevice::BeaconSeed>::
    Read(ash::multidevice::mojom::BeaconSeedDataView in,
         ash::multidevice::BeaconSeed* out) {
  std::string beacon_seed_data;
  base::Time start_time;
  base::Time end_time;

  if (!in.ReadData(&beacon_seed_data) || !in.ReadStartTime(&start_time) ||
      !in.ReadEndTime(&end_time)) {
    return false;
  }

  *out = ash::multidevice::BeaconSeed(beacon_seed_data, start_time, end_time);

  return true;
}

std::string StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                         ash::multidevice::RemoteDevice>::
    device_id(const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.GetDeviceId();
}

const std::string& StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                                ash::multidevice::RemoteDevice>::
    user_email(const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.user_email;
}

const std::string& StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                                ash::multidevice::RemoteDevice>::
    instance_id(const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.instance_id;
}

const std::string& StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                                ash::multidevice::RemoteDevice>::
    device_name(const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.name;
}

const std::string& StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                                ash::multidevice::RemoteDevice>::
    pii_free_device_name(const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.pii_free_name;
}

const std::string& StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                                ash::multidevice::RemoteDevice>::
    persistent_symmetric_key(
        const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.persistent_symmetric_key;
}

base::Time StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                        ash::multidevice::RemoteDevice>::
    last_update_time(const ash::multidevice::RemoteDevice& remote_device) {
  return base::Time::FromMillisecondsSinceUnixEpoch(
      remote_device.last_update_time_millis);
}

const std::map<ash::multidevice::SoftwareFeature,
               ash::multidevice::SoftwareFeatureState>&
StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
             ash::multidevice::RemoteDevice>::
    software_features(const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.software_features;
}

const std::vector<ash::multidevice::BeaconSeed>&
StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
             ash::multidevice::RemoteDevice>::
    beacon_seeds(const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.beacon_seeds;
}

const std::string& StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                                ash::multidevice::RemoteDevice>::
    bluetooth_public_address(
        const ash::multidevice::RemoteDevice& remote_device) {
  return remote_device.bluetooth_public_address;
}

bool StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                  ash::multidevice::RemoteDevice>::
    Read(ash::multidevice::mojom::RemoteDeviceDataView in,
         ash::multidevice::RemoteDevice* out) {
  std::string device_id;
  base::Time last_update_time;

  if (!in.ReadUserEmail(&out->user_email) ||
      !in.ReadInstanceId(&out->instance_id) || !in.ReadDeviceName(&out->name) ||
      !in.ReadPiiFreeDeviceName(&out->pii_free_name) ||
      !in.ReadDeviceId(&device_id) ||
      !in.ReadPersistentSymmetricKey(&out->persistent_symmetric_key) ||
      !in.ReadLastUpdateTime(&last_update_time) ||
      !in.ReadSoftwareFeatures(&out->software_features) ||
      !in.ReadBeaconSeeds(&out->beacon_seeds) ||
      !in.ReadBluetoothPublicAddress(&out->bluetooth_public_address)) {
    return false;
  }

  // Note: |bluetooth_public_address| may be empty if it has not been synced.
  if (!out->bluetooth_public_address.empty()) {
    std::string bluetooth_public_address_before_canonicalizing =
        out->bluetooth_public_address;

    // Canonicalize address, which capitalizes all hex digits. Note that if the
    // input address is invalid, CanonicalizeAddress() returns an empty string.
    out->bluetooth_public_address =
        device::CanonicalizeBluetoothAddress(out->bluetooth_public_address);

    if (out->bluetooth_public_address.empty()) {
      PA_LOG(ERROR) << "Invalid bluetooth public address \""
                    << bluetooth_public_address_before_canonicalizing
                    << "\" for device with ID \"" << out->GetDeviceId()
                    << "\"; clearing.";
    }
  }

  out->public_key = ash::multidevice::RemoteDevice::DerivePublicKey(device_id);
  out->last_update_time_millis =
      last_update_time.InMillisecondsSinceUnixEpoch();

  return true;
}

ash::multidevice::mojom::SoftwareFeature
EnumTraits<ash::multidevice::mojom::SoftwareFeature,
           ash::multidevice::SoftwareFeature>::
    ToMojom(ash::multidevice::SoftwareFeature input) {
  switch (input) {
    case ash::multidevice::SoftwareFeature::kBetterTogetherHost:
      return ash::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_HOST;
    case ash::multidevice::SoftwareFeature::kBetterTogetherClient:
      return ash::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_CLIENT;
    case ash::multidevice::SoftwareFeature::kSmartLockHost:
      return ash::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_HOST;
    case ash::multidevice::SoftwareFeature::kSmartLockClient:
      return ash::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_CLIENT;
    case ash::multidevice::SoftwareFeature::kInstantTetheringHost:
      return ash::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_HOST;
    case ash::multidevice::SoftwareFeature::kInstantTetheringClient:
      return ash::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_CLIENT;
    case ash::multidevice::SoftwareFeature::kMessagesForWebHost:
      return ash::multidevice::mojom::SoftwareFeature::SMS_CONNECT_HOST;
    case ash::multidevice::SoftwareFeature::kMessagesForWebClient:
      return ash::multidevice::mojom::SoftwareFeature::SMS_CONNECT_CLIENT;
    case ash::multidevice::SoftwareFeature::kPhoneHubHost:
      return ash::multidevice::mojom::SoftwareFeature::PHONE_HUB_HOST;
    case ash::multidevice::SoftwareFeature::kPhoneHubClient:
      return ash::multidevice::mojom::SoftwareFeature::PHONE_HUB_CLIENT;
    case ash::multidevice::SoftwareFeature::kWifiSyncHost:
      return ash::multidevice::mojom::SoftwareFeature::WIFI_SYNC_HOST;
    case ash::multidevice::SoftwareFeature::kWifiSyncClient:
      return ash::multidevice::mojom::SoftwareFeature::WIFI_SYNC_CLIENT;
    case ash::multidevice::SoftwareFeature::kEcheHost:
      return ash::multidevice::mojom::SoftwareFeature::ECHE_HOST;
    case ash::multidevice::SoftwareFeature::kEcheClient:
      return ash::multidevice::mojom::SoftwareFeature::ECHE_CLIENT;
    case ash::multidevice::SoftwareFeature::kPhoneHubCameraRollHost:
      return ash::multidevice::mojom::SoftwareFeature::
          PHONE_HUB_CAMERA_ROLL_HOST;
    case ash::multidevice::SoftwareFeature::kPhoneHubCameraRollClient:
      return ash::multidevice::mojom::SoftwareFeature::
          PHONE_HUB_CAMERA_ROLL_CLIENT;
  }

  NOTREACHED_IN_MIGRATION();
  return ash::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_HOST;
}

bool EnumTraits<ash::multidevice::mojom::SoftwareFeature,
                ash::multidevice::SoftwareFeature>::
    FromMojom(ash::multidevice::mojom::SoftwareFeature input,
              ash::multidevice::SoftwareFeature* out) {
  switch (input) {
    case ash::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_HOST:
      *out = ash::multidevice::SoftwareFeature::kBetterTogetherHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kBetterTogetherClient;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_HOST:
      *out = ash::multidevice::SoftwareFeature::kSmartLockHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kSmartLockClient;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_HOST:
      *out = ash::multidevice::SoftwareFeature::kInstantTetheringHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kInstantTetheringClient;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::SMS_CONNECT_HOST:
      *out = ash::multidevice::SoftwareFeature::kMessagesForWebHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::SMS_CONNECT_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kMessagesForWebClient;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::PHONE_HUB_HOST:
      *out = ash::multidevice::SoftwareFeature::kPhoneHubHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::PHONE_HUB_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kPhoneHubClient;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::WIFI_SYNC_HOST:
      *out = ash::multidevice::SoftwareFeature::kWifiSyncHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::WIFI_SYNC_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kWifiSyncClient;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::ECHE_HOST:
      *out = ash::multidevice::SoftwareFeature::kEcheHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::ECHE_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kEcheClient;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_HOST:
      *out = ash::multidevice::SoftwareFeature::kPhoneHubCameraRollHost;
      return true;
    case ash::multidevice::mojom::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_CLIENT:
      *out = ash::multidevice::SoftwareFeature::kPhoneHubCameraRollClient;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

ash::multidevice::mojom::SoftwareFeatureState
EnumTraits<ash::multidevice::mojom::SoftwareFeatureState,
           ash::multidevice::SoftwareFeatureState>::
    ToMojom(ash::multidevice::SoftwareFeatureState input) {
  switch (input) {
    case ash::multidevice::SoftwareFeatureState::kNotSupported:
      return ash::multidevice::mojom::SoftwareFeatureState::kNotSupported;
    case ash::multidevice::SoftwareFeatureState::kSupported:
      return ash::multidevice::mojom::SoftwareFeatureState::kSupported;
    case ash::multidevice::SoftwareFeatureState::kEnabled:
      return ash::multidevice::mojom::SoftwareFeatureState::kEnabled;
  }

  NOTREACHED_IN_MIGRATION();
  return ash::multidevice::mojom::SoftwareFeatureState::kNotSupported;
}

bool EnumTraits<ash::multidevice::mojom::SoftwareFeatureState,
                ash::multidevice::SoftwareFeatureState>::
    FromMojom(ash::multidevice::mojom::SoftwareFeatureState input,
              ash::multidevice::SoftwareFeatureState* out) {
  switch (input) {
    case ash::multidevice::mojom::SoftwareFeatureState::kNotSupported:
      *out = ash::multidevice::SoftwareFeatureState::kNotSupported;
      return true;
    case ash::multidevice::mojom::SoftwareFeatureState::kSupported:
      *out = ash::multidevice::SoftwareFeatureState::kSupported;
      return true;
    case ash::multidevice::mojom::SoftwareFeatureState::kEnabled:
      *out = ash::multidevice::SoftwareFeatureState::kEnabled;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
