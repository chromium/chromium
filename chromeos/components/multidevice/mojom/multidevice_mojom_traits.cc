// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/mojom/multidevice_mojom_traits.h"

#include "base/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

const std::string&
StructTraits<chromeos::multidevice::mojom::BeaconSeedDataView,
             chromeos::multidevice::BeaconSeed>::
    data(const chromeos::multidevice::BeaconSeed& beacon_seed) {
  return beacon_seed.data();
}

base::Time StructTraits<chromeos::multidevice::mojom::BeaconSeedDataView,
                        chromeos::multidevice::BeaconSeed>::
    start_time(const chromeos::multidevice::BeaconSeed& beacon_seed) {
  return beacon_seed.start_time();
}

base::Time StructTraits<chromeos::multidevice::mojom::BeaconSeedDataView,
                        chromeos::multidevice::BeaconSeed>::
    end_time(const chromeos::multidevice::BeaconSeed& beacon_seed) {
  return beacon_seed.end_time();
}

bool StructTraits<chromeos::multidevice::mojom::BeaconSeedDataView,
                  chromeos::multidevice::BeaconSeed>::
    Read(chromeos::multidevice::mojom::BeaconSeedDataView in,
         chromeos::multidevice::BeaconSeed* out) {
  std::string beacon_seed_data;
  base::Time start_time;
  base::Time end_time;

  if (!in.ReadData(&beacon_seed_data) || !in.ReadStartTime(&start_time) ||
      !in.ReadEndTime(&end_time)) {
    return false;
  }

  *out =
      chromeos::multidevice::BeaconSeed(beacon_seed_data, start_time, end_time);

  return true;
}

std::string StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
                         chromeos::multidevice::RemoteDevice>::
    device_id(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.GetDeviceId();
}

const std::string&
StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    user_id(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.user_id;
}

const std::string&
StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    instance_id(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.instance_id;
}

const std::string&
StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    device_name(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.name;
}

const std::string&
StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    pii_free_device_name(
        const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.pii_free_name;
}

const std::string&
StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    persistent_symmetric_key(
        const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.persistent_symmetric_key;
}

base::Time StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
                        chromeos::multidevice::RemoteDevice>::
    last_update_time(const chromeos::multidevice::RemoteDevice& remote_device) {
  return base::Time::FromJavaTime(remote_device.last_update_time_millis);
}

const std::map<chromeos::multidevice::SoftwareFeature,
               chromeos::multidevice::SoftwareFeatureState>&
StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    software_features(
        const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.software_features;
}

const std::vector<chromeos::multidevice::BeaconSeed>&
StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    beacon_seeds(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.beacon_seeds;
}

bool StructTraits<chromeos::multidevice::mojom::RemoteDeviceDataView,
                  chromeos::multidevice::RemoteDevice>::
    Read(chromeos::multidevice::mojom::RemoteDeviceDataView in,
         chromeos::multidevice::RemoteDevice* out) {
  std::string device_id;
  base::Time last_update_time;

  if (!in.ReadUserId(&out->user_id) || !in.ReadInstanceId(&out->instance_id) ||
      !in.ReadDeviceName(&out->name) ||
      !in.ReadPiiFreeDeviceName(&out->pii_free_name) ||
      !in.ReadDeviceId(&device_id) ||
      !in.ReadPersistentSymmetricKey(&out->persistent_symmetric_key) ||
      !in.ReadLastUpdateTime(&last_update_time) ||
      !in.ReadSoftwareFeatures(&out->software_features) ||
      !in.ReadBeaconSeeds(&out->beacon_seeds)) {
    return false;
  }

  out->public_key =
      chromeos::multidevice::RemoteDevice::DerivePublicKey(device_id);
  out->last_update_time_millis = last_update_time.ToJavaTime();

  return true;
}

chromeos::multidevice::mojom::SoftwareFeature
EnumTraits<chromeos::multidevice::mojom::SoftwareFeature,
           chromeos::multidevice::SoftwareFeature>::
    ToMojom(chromeos::multidevice::SoftwareFeature input) {
  switch (input) {
    case chromeos::multidevice::SoftwareFeature::kBetterTogetherHost:
      return chromeos::multidevice::mojom::SoftwareFeature::
          BETTER_TOGETHER_HOST;
    case chromeos::multidevice::SoftwareFeature::kBetterTogetherClient:
      return chromeos::multidevice::mojom::SoftwareFeature::
          BETTER_TOGETHER_CLIENT;
    case chromeos::multidevice::SoftwareFeature::kSmartLockHost:
      return chromeos::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_HOST;
    case chromeos::multidevice::SoftwareFeature::kSmartLockClient:
      return chromeos::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_CLIENT;
    case chromeos::multidevice::SoftwareFeature::kInstantTetheringHost:
      return chromeos::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_HOST;
    case chromeos::multidevice::SoftwareFeature::kInstantTetheringClient:
      return chromeos::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_CLIENT;
    case chromeos::multidevice::SoftwareFeature::kMessagesForWebHost:
      return chromeos::multidevice::mojom::SoftwareFeature::SMS_CONNECT_HOST;
    case chromeos::multidevice::SoftwareFeature::kMessagesForWebClient:
      return chromeos::multidevice::mojom::SoftwareFeature::SMS_CONNECT_CLIENT;
  }

  NOTREACHED();
  return chromeos::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_HOST;
}

bool EnumTraits<chromeos::multidevice::mojom::SoftwareFeature,
                chromeos::multidevice::SoftwareFeature>::
    FromMojom(chromeos::multidevice::mojom::SoftwareFeature input,
              chromeos::multidevice::SoftwareFeature* out) {
  switch (input) {
    case chromeos::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_HOST:
      *out = chromeos::multidevice::SoftwareFeature::kBetterTogetherHost;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      *out = chromeos::multidevice::SoftwareFeature::kBetterTogetherClient;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_HOST:
      *out = chromeos::multidevice::SoftwareFeature::kSmartLockHost;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeature::EASY_UNLOCK_CLIENT:
      *out = chromeos::multidevice::SoftwareFeature::kSmartLockClient;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_HOST:
      *out = chromeos::multidevice::SoftwareFeature::kInstantTetheringHost;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeature::MAGIC_TETHER_CLIENT:
      *out = chromeos::multidevice::SoftwareFeature::kInstantTetheringClient;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeature::SMS_CONNECT_HOST:
      *out = chromeos::multidevice::SoftwareFeature::kMessagesForWebHost;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeature::SMS_CONNECT_CLIENT:
      *out = chromeos::multidevice::SoftwareFeature::kMessagesForWebClient;
      return true;
  }

  NOTREACHED();
  return false;
}

chromeos::multidevice::mojom::SoftwareFeatureState
EnumTraits<chromeos::multidevice::mojom::SoftwareFeatureState,
           chromeos::multidevice::SoftwareFeatureState>::
    ToMojom(chromeos::multidevice::SoftwareFeatureState input) {
  switch (input) {
    case chromeos::multidevice::SoftwareFeatureState::kNotSupported:
      return chromeos::multidevice::mojom::SoftwareFeatureState::kNotSupported;
    case chromeos::multidevice::SoftwareFeatureState::kSupported:
      return chromeos::multidevice::mojom::SoftwareFeatureState::kSupported;
    case chromeos::multidevice::SoftwareFeatureState::kEnabled:
      return chromeos::multidevice::mojom::SoftwareFeatureState::kEnabled;
  }

  NOTREACHED();
  return chromeos::multidevice::mojom::SoftwareFeatureState::kNotSupported;
}

bool EnumTraits<chromeos::multidevice::mojom::SoftwareFeatureState,
                chromeos::multidevice::SoftwareFeatureState>::
    FromMojom(chromeos::multidevice::mojom::SoftwareFeatureState input,
              chromeos::multidevice::SoftwareFeatureState* out) {
  switch (input) {
    case chromeos::multidevice::mojom::SoftwareFeatureState::kNotSupported:
      *out = chromeos::multidevice::SoftwareFeatureState::kNotSupported;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeatureState::kSupported:
      *out = chromeos::multidevice::SoftwareFeatureState::kSupported;
      return true;
    case chromeos::multidevice::mojom::SoftwareFeatureState::kEnabled:
      *out = chromeos::multidevice::SoftwareFeatureState::kEnabled;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
