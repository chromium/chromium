// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/remote_device_loader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"

namespace ash {

namespace device_sync {

namespace {

std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
GetSoftwareFeatureToStateMap(const cryptauth::ExternalDeviceInfo& device) {
  std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
      software_feature_to_state_map;

  for (int i = 0; i < device.supported_software_features_size(); ++i) {
    cryptauth::SoftwareFeature feature =
        SoftwareFeatureStringToEnum(device.supported_software_features(i));
    if (feature == cryptauth::UNKNOWN_FEATURE)
      continue;

    software_feature_to_state_map[multidevice::FromCryptAuthFeature(feature)] =
        multidevice::SoftwareFeatureState::kSupported;
  }

  for (int i = 0; i < device.enabled_software_features_size(); ++i) {
    cryptauth::SoftwareFeature feature =
        SoftwareFeatureStringToEnum(device.enabled_software_features(i));
    if (feature == cryptauth::UNKNOWN_FEATURE)
      continue;

    software_feature_to_state_map[multidevice::FromCryptAuthFeature(feature)] =
        multidevice::SoftwareFeatureState::kEnabled;
  }

  return software_feature_to_state_map;
}

}  // namespace

// static
RemoteDeviceLoader::Factory* RemoteDeviceLoader::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<RemoteDeviceLoader> RemoteDeviceLoader::Factory::Create(
    const std::vector<cryptauth::ExternalDeviceInfo>& device_info_list,
    const std::string& user_email,
    const std::string& user_private_key,
    std::unique_ptr<multidevice::SecureMessageDelegate>
        secure_message_delegate) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        device_info_list, user_email, user_private_key,
        std::move(secure_message_delegate));
  }

  return base::WrapUnique(
      new RemoteDeviceLoader(device_info_list, user_email, user_private_key,
                             std::move(secure_message_delegate)));
}

// static
void RemoteDeviceLoader::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

RemoteDeviceLoader::RemoteDeviceLoader(
    const std::vector<cryptauth::ExternalDeviceInfo>& device_info_list,
    const std::string& user_email,
    const std::string& user_private_key,
    std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate)
    : remaining_devices_(device_info_list),
      user_email_(user_email),
      user_private_key_(user_private_key),
      secure_message_delegate_(std::move(secure_message_delegate)) {}

RemoteDeviceLoader::~RemoteDeviceLoader() {}

void RemoteDeviceLoader::Load(RemoteDeviceCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
  PA_LOG(VERBOSE) << "Loading " << remaining_devices_.size()
                  << " remote devices";

  if (remaining_devices_.empty()) {
    std::move(callback_).Run(remote_devices_);
    return;
  }

  std::vector<cryptauth::ExternalDeviceInfo> all_devices_to_convert =
      remaining_devices_;

  for (const auto& device : all_devices_to_convert) {
    secure_message_delegate_->DeriveKey(
        user_private_key_, device.public_key(),
        base::BindOnce(&RemoteDeviceLoader::OnPSKDerived,
                       weak_ptr_factory_.GetWeakPtr(), device));
  }
}

void RemoteDeviceLoader::OnPSKDerived(
    const cryptauth::ExternalDeviceInfo& device,
    const std::string& psk) {
  auto iterator =
      base::ranges::find(remaining_devices_, device.public_key(),
                         &cryptauth::ExternalDeviceInfo::public_key);

  DCHECK(iterator != remaining_devices_.end());
  remaining_devices_.erase(iterator);

  std::vector<multidevice::BeaconSeed> multidevice_beacon_seeds;
  for (const auto& cryptauth_beacon_seed : device.beacon_seeds()) {
    multidevice_beacon_seeds.push_back(
        multidevice::FromCryptAuthSeed(cryptauth_beacon_seed));
  }

  // Because RemoteDeviceLoader does not handle devices using v2 DeviceSync, no
  // Instance ID is present.
  multidevice::RemoteDevice remote_device(
      user_email_, std::string() /* instance_id */,
      device.friendly_device_name(), device.no_pii_device_name(),
      device.public_key(), psk, device.last_update_time_millis(),
      GetSoftwareFeatureToStateMap(device), multidevice_beacon_seeds,
      device.bluetooth_address());

  remote_devices_.push_back(remote_device);

  if (remaining_devices_.empty()) {
    PA_LOG(VERBOSE) << "Derived keys for " << remote_devices_.size()
                    << " devices.";
    std::move(callback_).Run(remote_devices_);
  }
}

}  // namespace device_sync

}  // namespace ash
