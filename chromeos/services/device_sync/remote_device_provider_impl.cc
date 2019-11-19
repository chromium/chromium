// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/remote_device_provider_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/remote_device_loader.h"

namespace chromeos {

namespace device_sync {

// static
RemoteDeviceProviderImpl::Factory*
    RemoteDeviceProviderImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<RemoteDeviceProvider>
RemoteDeviceProviderImpl::Factory::NewInstance(
    CryptAuthDeviceManager* device_manager,
    const std::string& user_id,
    const std::string& user_private_key) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(device_manager, user_id,
                                          user_private_key);
}

// static
void RemoteDeviceProviderImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

RemoteDeviceProviderImpl::Factory::~Factory() = default;

std::unique_ptr<RemoteDeviceProvider>
RemoteDeviceProviderImpl::Factory::BuildInstance(
    CryptAuthDeviceManager* device_manager,
    const std::string& user_id,
    const std::string& user_private_key) {
  return base::WrapUnique(
      new RemoteDeviceProviderImpl(device_manager, user_id, user_private_key));
}

RemoteDeviceProviderImpl::RemoteDeviceProviderImpl(
    CryptAuthDeviceManager* device_manager,
    const std::string& user_id,
    const std::string& user_private_key)
    : device_manager_(device_manager),
      user_id_(user_id),
      user_private_key_(user_private_key) {
  device_manager_->AddObserver(this);
  remote_device_loader_ = RemoteDeviceLoader::Factory::NewInstance(
      device_manager->GetSyncedDevices(), user_id, user_private_key,
      multidevice::SecureMessageDelegateImpl::Factory::NewInstance());
  remote_device_loader_->Load(
      base::Bind(&RemoteDeviceProviderImpl::OnRemoteDevicesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

RemoteDeviceProviderImpl::~RemoteDeviceProviderImpl() {
  device_manager_->RemoveObserver(this);
}

void RemoteDeviceProviderImpl::OnSyncFinished(
    CryptAuthDeviceManager::SyncResult sync_result,
    CryptAuthDeviceManager::DeviceChangeResult device_change_result) {
  if (sync_result == CryptAuthDeviceManager::SyncResult::SUCCESS &&
      device_change_result ==
          CryptAuthDeviceManager::DeviceChangeResult::CHANGED) {
    remote_device_loader_ = RemoteDeviceLoader::Factory::NewInstance(
        device_manager_->GetSyncedDevices(), user_id_, user_private_key_,
        multidevice::SecureMessageDelegateImpl::Factory::NewInstance());

    remote_device_loader_->Load(
        base::Bind(&RemoteDeviceProviderImpl::OnRemoteDevicesLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void RemoteDeviceProviderImpl::OnRemoteDevicesLoaded(
    const multidevice::RemoteDeviceList& synced_remote_devices) {
  synced_remote_devices_ = synced_remote_devices;
  remote_device_loader_.reset();

  // Notify observers of change. Note that there is no need to check if
  // |synced_remote_devices_| has changed here because the fetch is only started
  // if the change result passed to OnSyncFinished() is CHANGED.
  RemoteDeviceProvider::NotifyObserversDeviceListChanged();
}

const multidevice::RemoteDeviceList&
RemoteDeviceProviderImpl::GetSyncedDevices() const {
  return synced_remote_devices_;
}

}  // namespace device_sync

}  // namespace chromeos
