// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host_fetcher_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/remote_device.h"

namespace ash {

namespace tether {

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

  CacheCurrentTetherHosts();
}

TetherHostFetcherImpl::~TetherHostFetcherImpl() {
  device_sync_client_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

bool TetherHostFetcherImpl::HasSyncedTetherHosts() {
  return !current_remote_device_list_.empty();
}

void TetherHostFetcherImpl::FetchAllTetherHosts(
    TetherHostListCallback callback) {
  ProcessFetchAllTetherHostsRequest(current_remote_device_list_,
                                    std::move(callback));
}

void TetherHostFetcherImpl::FetchTetherHost(const std::string& device_id,
                                            TetherHostCallback callback) {
  ProcessFetchSingleTetherHostRequest(device_id, current_remote_device_list_,
                                      std::move(callback));
}

void TetherHostFetcherImpl::OnNewDevicesSynced() {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::OnReady() {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::CacheCurrentTetherHosts() {
  multidevice::RemoteDeviceRefList updated_list = GenerateHostDeviceList();
  if (updated_list == current_remote_device_list_)
    return;

  current_remote_device_list_.swap(updated_list);
  NotifyTetherHostsUpdated();
}

multidevice::RemoteDeviceRefList
TetherHostFetcherImpl::GenerateHostDeviceList() {
  multidevice::RemoteDeviceRefList host_list;

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device = multidevice_setup_client_->GetHostStatus();
  if (host_status_with_device.first ==
      multidevice_setup::mojom::HostStatus::kHostVerified) {
    host_list.push_back(*host_status_with_device.second);
  }
  return host_list;
}

}  // namespace tether

}  // namespace ash
