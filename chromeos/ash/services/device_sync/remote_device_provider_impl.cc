// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/remote_device_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/device_sync/remote_device_loader.h"
#include "chromeos/ash/services/device_sync/remote_device_v2_loader_impl.h"

namespace ash::device_sync {

namespace {

const int kMaxNumberOfDevicesToLog = 50;

// Only used while v1 and v2 DeviceSync are running in parallel.
void LogRemoteDeviceCountMetrics(
    const multidevice::RemoteDeviceList& v1_devices,
    const multidevice::RemoteDeviceList& v2_devices,
    size_t num_v2_devices_with_decrypted_public_key,
    size_t num_v1_devices_replaced_by_v2_devices) {
  // At a minimum, the local device should always be returned from a successful
  // v1 or v2 DeviceSync. Only log metrics if v1 and v2 devices are available,
  // in other words, if a v1 *and* v2 DeviceSync has previously occurred.
  if (v1_devices.empty() || v2_devices.empty())
    return;

  base::UmaHistogramExactLinear(
      "CryptAuth.DeviceSyncV2.RemoteDeviceProvider.NumV1Devices",
      v1_devices.size(), kMaxNumberOfDevicesToLog);
  base::UmaHistogramExactLinear(
      "CryptAuth.DeviceSyncV2.RemoteDeviceProvider.NumV2Devices",
      v2_devices.size(), kMaxNumberOfDevicesToLog);

  // Note: By CryptAuth server design, v2 devices should always be a subset of
  // v1 devices. Race conditions might occur when a user adds a new device
  // because v1 and v2 devices are retrieved in different RPC calls; however,
  // this is only meant to be a rough estimate.
  base::UmaHistogramPercentageObsoleteDoNotUse(
      "CryptAuth.DeviceSyncV2.RemoteDeviceProvider.RatioOfV2ToV1Devices",
      (v2_devices.size() * 100) / v1_devices.size());

  base::UmaHistogramPercentageObsoleteDoNotUse(
      "CryptAuth.DeviceSyncV2.RemoteDeviceProvider."
      "PercentageOfV2DevicesWithDecryptedPublicKey",
      (num_v2_devices_with_decrypted_public_key * 100) / v2_devices.size());

  base::UmaHistogramPercentageObsoleteDoNotUse(
      "CryptAuth.DeviceSyncV2.RemoteDeviceProvider."
      "PercentageOfV1DevicesReplacedByV2Devices",
      (num_v1_devices_replaced_by_v2_devices * 100) / v1_devices.size());
}

}  // namespace

// static
RemoteDeviceProviderImpl::Factory*
    RemoteDeviceProviderImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<RemoteDeviceProvider> RemoteDeviceProviderImpl::Factory::Create(
    CryptAuthV2DeviceManager* v2_device_manager,
    const std::string& user_email,
    const std::string& user_private_key) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(v2_device_manager, user_email,
                                             user_private_key);
  }

  return base::WrapUnique(new RemoteDeviceProviderImpl(
      v2_device_manager, user_email, user_private_key));
}

// static
void RemoteDeviceProviderImpl::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

RemoteDeviceProviderImpl::Factory::~Factory() = default;

RemoteDeviceProviderImpl::RemoteDeviceProviderImpl(
    CryptAuthV2DeviceManager* v2_device_manager,
    const std::string& user_email,
    const std::string& user_private_key)
    : v2_device_manager_(v2_device_manager),
      user_email_(user_email),
      user_private_key_(user_private_key) {
  if (features::ShouldUseV2DeviceSync()) {
    DCHECK(v2_device_manager_);
    v2_device_manager_->AddObserver(this);
    LoadV2RemoteDevices();
  }
}

RemoteDeviceProviderImpl::~RemoteDeviceProviderImpl() {
  if (v2_device_manager_)
    v2_device_manager_->RemoveObserver(this);
}

void RemoteDeviceProviderImpl::OnDeviceSyncFinished(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  DCHECK(features::ShouldUseV2DeviceSync());

  if (device_sync_result.IsSuccess() &&
      device_sync_result.did_device_registry_change()) {
    LoadV2RemoteDevices();
  }
}

void RemoteDeviceProviderImpl::LoadV2RemoteDevices() {
  remote_device_v2_loader_ = RemoteDeviceV2LoaderImpl::Factory::Create();
  remote_device_v2_loader_->Load(
      v2_device_manager_->GetSyncedDevices(), user_email_, user_private_key_,
      base::BindOnce(&RemoteDeviceProviderImpl::OnV2RemoteDevicesLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteDeviceProviderImpl::OnV1RemoteDevicesLoaded(
    const multidevice::RemoteDeviceList& synced_v1_remote_devices) {
  // If we are only using v1 DeviceSync, the complete list of RemoteDevices
  // is |synced_v1_remote_devices|.
  if (!features::ShouldUseV2DeviceSync()) {
    synced_remote_devices_ = synced_v1_remote_devices;
    remote_device_v1_loader_.reset();

    // Notify observers of change. Note that there is no need to check if
    // |synced_remote_devices_| has changed here because the fetch is only
    // started if the change result passed to OnSyncFinished() is CHANGED.
    RemoteDeviceProvider::NotifyObserversDeviceListChanged();
    return;
  }

  synced_v1_remote_devices_to_be_merged_ = synced_v1_remote_devices;
  remote_device_v1_loader_.reset();
  MergeV1andV2SyncedDevices();
}

void RemoteDeviceProviderImpl::OnV2RemoteDevicesLoaded(
    const multidevice::RemoteDeviceList& synced_v2_remote_devices) {
  // If we are only using v2 DeviceSync, the complete list of RemoteDevices
  // is |synced_v2_remote_devices|.
  if (!features::ShouldUseV1DeviceSync()) {
    synced_remote_devices_ = synced_v2_remote_devices;
    remote_device_v2_loader_.reset();

    // Notify observers of change. Note that there is no need to check if
    // |synced_remote_devices_| has changed here because the fetch is only
    // started if the DeviceSync result passed to OnDeviceSyncFinished()
    // indicates that the device registry changed.
    RemoteDeviceProvider::NotifyObserversDeviceListChanged();
    return;
  }

  synced_v2_remote_devices_to_be_merged_ = synced_v2_remote_devices;
  remote_device_v2_loader_.reset();
  MergeV1andV2SyncedDevices();
}

void RemoteDeviceProviderImpl::MergeV1andV2SyncedDevices() {
  DCHECK(features::ShouldUseV1DeviceSync());
  DCHECK(features::ShouldUseV2DeviceSync());

  multidevice::RemoteDeviceList previous_synced_remote_devices =
      synced_remote_devices_;

  synced_remote_devices_ = synced_v1_remote_devices_to_be_merged_;
  size_t num_v2_devices_with_decrypted_public_key = 0;
  size_t num_v1_devices_replaced_by_v2_devices = 0;
  for (const auto& v2_device : synced_v2_remote_devices_to_be_merged_) {
    // Ignore v2 devices without a decrypted public key.
    if (v2_device.public_key.empty())
      continue;

    ++num_v2_devices_with_decrypted_public_key;

    std::string v2_public_key = v2_device.public_key;
    auto it = base::ranges::find(synced_remote_devices_, v2_public_key,
                                 &multidevice::RemoteDevice::public_key);

    // If a v1 device has the same public key as the v2 device, replace the
    // v1 device with the v2 device; otherwise, append the v2 device to the
    // synced-device list.
    if (it != synced_remote_devices_.end()) {
      *it = v2_device;
      ++num_v1_devices_replaced_by_v2_devices;
    } else {
      synced_remote_devices_.push_back(v2_device);
    }
  }

  LogRemoteDeviceCountMetrics(synced_v1_remote_devices_to_be_merged_,
                              synced_v2_remote_devices_to_be_merged_,
                              num_v2_devices_with_decrypted_public_key,
                              num_v1_devices_replaced_by_v2_devices);

  // We need to explicitly check for changes to the synced-device list. It
  // is possible that the v1 and/or v2 device lists changed but the merged
  // list didn't change, for example, if a new v2 device appears in the
  // device registry but it doesn't have a decrypted public key.
  if (synced_remote_devices_ != previous_synced_remote_devices)
    RemoteDeviceProvider::NotifyObserversDeviceListChanged();
}

const multidevice::RemoteDeviceList&
RemoteDeviceProviderImpl::GetSyncedDevices() const {
  return synced_remote_devices_;
}

}  // namespace ash::device_sync
