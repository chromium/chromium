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

void RemoteDeviceProviderImpl::OnV2RemoteDevicesLoaded(
    const multidevice::RemoteDeviceList& synced_v2_remote_devices) {
  synced_remote_devices_ = synced_v2_remote_devices;
  remote_device_v2_loader_.reset();

  // Notify observers of change. Note that there is no need to check if
  // |synced_remote_devices_| has changed here because the fetch is only
  // started if the DeviceSync result passed to OnDeviceSyncFinished()
  // indicates that the device registry changed.
  RemoteDeviceProvider::NotifyObserversDeviceListChanged();
}

const multidevice::RemoteDeviceList&
RemoteDeviceProviderImpl::GetSyncedDevices() const {
  return synced_remote_devices_;
}

}  // namespace ash::device_sync
