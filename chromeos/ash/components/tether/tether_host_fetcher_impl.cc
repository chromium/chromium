// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host_fetcher_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/remote_device.h"

namespace ash::tether {

// static
TetherHostFetcherImpl::Factory*
    TetherHostFetcherImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<TetherHostFetcher> TetherHostFetcherImpl::Factory::Create(
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(device_sync_client,
                                             multidevice_setup_client);
  }

  return base::WrapUnique(
      new TetherHostFetcherImpl(device_sync_client, multidevice_setup_client));
}

// static
void TetherHostFetcherImpl::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

TetherHostFetcherImpl::TetherHostFetcherImpl(
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client) {
  device_sync_client_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);

  CacheCurrentTetherHost();
}

TetherHostFetcherImpl::~TetherHostFetcherImpl() {
  device_sync_client_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

void TetherHostFetcherImpl::OnNewDevicesSynced() {
  CacheCurrentTetherHost();
}

void TetherHostFetcherImpl::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  CacheCurrentTetherHost();
}

void TetherHostFetcherImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  CacheCurrentTetherHost();
}

void TetherHostFetcherImpl::OnReady() {
  CacheCurrentTetherHost();
}

void TetherHostFetcherImpl::CacheCurrentTetherHost() {
  std::optional<multidevice::RemoteDeviceRef> updated_tether_host =
      GenerateTetherHost();
  if (updated_tether_host == tether_host_) {
    return;
  }

  tether_host_ = updated_tether_host;
  NotifyTetherHostUpdated();
}

std::optional<multidevice::RemoteDeviceRef>
TetherHostFetcherImpl::GenerateTetherHost() {
  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device = multidevice_setup_client_->GetHostStatus();
  if (host_status_with_device.first ==
      multidevice_setup::mojom::HostStatus::kHostVerified) {
    return *host_status_with_device.second;
  }
  return std::nullopt;
}

}  // namespace ash::tether
