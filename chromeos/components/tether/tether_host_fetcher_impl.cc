// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_host_fetcher_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/chromeos_features.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_provider.h"

namespace chromeos {

namespace tether {

// static
TetherHostFetcherImpl::Factory*
    TetherHostFetcherImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<TetherHostFetcher> TetherHostFetcherImpl::Factory::NewInstance(
    cryptauth::RemoteDeviceProvider* remote_device_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(
      remote_device_provider, device_sync_client, multidevice_setup_client);
}

// static
void TetherHostFetcherImpl::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<TetherHostFetcher>
TetherHostFetcherImpl::Factory::BuildInstance(
    cryptauth::RemoteDeviceProvider* remote_device_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client) {
  return base::WrapUnique(new TetherHostFetcherImpl(
      remote_device_provider, device_sync_client, multidevice_setup_client));
}

TetherHostFetcherImpl::TetherHostFetcherImpl(
    cryptauth::RemoteDeviceProvider* remote_device_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client)
    : remote_device_provider_(remote_device_provider),
      device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client),
      weak_ptr_factory_(this) {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
    device_sync_client_->AddObserver(this);

  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    multidevice_setup_client_->AddObserver(this);
  }

  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
      !base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    remote_device_provider_->AddObserver(this);
  }

  CacheCurrentTetherHosts();
}

TetherHostFetcherImpl::~TetherHostFetcherImpl() {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
    device_sync_client_->RemoveObserver(this);

  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    multidevice_setup_client_->RemoveObserver(this);
  }

  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
      !base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    remote_device_provider_->RemoveObserver(this);
  }
}

bool TetherHostFetcherImpl::HasSyncedTetherHosts() {
  return !current_remote_device_list_.empty();
}

void TetherHostFetcherImpl::FetchAllTetherHosts(
    const TetherHostListCallback& callback) {
  ProcessFetchAllTetherHostsRequest(current_remote_device_list_, callback);
}

void TetherHostFetcherImpl::FetchTetherHost(
    const std::string& device_id,
    const TetherHostCallback& callback) {
  ProcessFetchSingleTetherHostRequest(device_id, current_remote_device_list_,
                                      callback);
}

void TetherHostFetcherImpl::OnSyncDeviceListChanged() {
  CacheCurrentTetherHosts();
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
  cryptauth::RemoteDeviceRefList updated_list = GenerateHostDeviceList();
  if (updated_list == current_remote_device_list_)
    return;

  current_remote_device_list_.swap(updated_list);
  NotifyTetherHostsUpdated();
}

cryptauth::RemoteDeviceRefList TetherHostFetcherImpl::GenerateHostDeviceList() {
  cryptauth::RemoteDeviceRefList host_list;

  TetherHostSource tether_host_source = GetTetherHostSourceBasedOnFlags();

  if (tether_host_source == TetherHostSource::MULTIDEVICE_SETUP_CLIENT) {
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
        host_status_with_device = multidevice_setup_client_->GetHostStatus();
    if (host_status_with_device.first ==
        chromeos::multidevice_setup::mojom::HostStatus::kHostVerified) {
      host_list.push_back(*host_status_with_device.second);
    }
    return host_list;
  }

  if (tether_host_source == TetherHostSource::DEVICE_SYNC_CLIENT) {
    for (const cryptauth::RemoteDeviceRef& remote_device_ref :
         device_sync_client_->GetSyncedDevices()) {
      cryptauth::SoftwareFeatureState magic_tether_host_state =
          remote_device_ref.GetSoftwareFeatureState(
              cryptauth::SoftwareFeature::MAGIC_TETHER_HOST);
      if (magic_tether_host_state ==
              cryptauth::SoftwareFeatureState::kSupported ||
          magic_tether_host_state ==
              cryptauth::SoftwareFeatureState::kEnabled) {
        host_list.push_back(remote_device_ref);
      }
    }
    return host_list;
  }

  if (tether_host_source == TetherHostSource::REMOTE_DEVICE_PROVIDER) {
    for (const cryptauth::RemoteDevice& remote_device :
         remote_device_provider_->GetSyncedDevices()) {
      if (base::ContainsKey(remote_device.software_features,
                            cryptauth::SoftwareFeature::MAGIC_TETHER_HOST) &&
          (remote_device.software_features.at(
               cryptauth::SoftwareFeature::MAGIC_TETHER_HOST) ==
               cryptauth::SoftwareFeatureState::kSupported ||
           remote_device.software_features.at(
               cryptauth::SoftwareFeature::MAGIC_TETHER_HOST) ==
               cryptauth::SoftwareFeatureState::kEnabled)) {
        host_list.push_back(cryptauth::RemoteDeviceRef(
            std::make_shared<cryptauth::RemoteDevice>(remote_device)));
      }
    }
    return host_list;
  }

  return host_list;
}

bool TetherHostFetcherImpl::IsInLegacyHostMode() {
  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) ||
      !device_sync_client_->is_ready()) {
    return false;
  }

  bool has_supported_tether_host = false;
  for (const cryptauth::RemoteDeviceRef& remote_device_ref :
       device_sync_client_->GetSyncedDevices()) {
    cryptauth::SoftwareFeatureState better_together_host_state =
        remote_device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
    // If there's any valid Better Together host, don't support legacy mode.
    if (better_together_host_state ==
            cryptauth::SoftwareFeatureState::kSupported ||
        better_together_host_state ==
            cryptauth::SoftwareFeatureState::kEnabled) {
      return false;
    }

    cryptauth::SoftwareFeatureState magic_tether_host_state =
        remote_device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::MAGIC_TETHER_HOST);
    if (magic_tether_host_state ==
            cryptauth::SoftwareFeatureState::kSupported ||
        magic_tether_host_state == cryptauth::SoftwareFeatureState::kEnabled) {
      has_supported_tether_host = true;
    }
  }

  return has_supported_tether_host;
}

TetherHostFetcherImpl::TetherHostSource
TetherHostFetcherImpl::GetTetherHostSourceBasedOnFlags() {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
      !base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    return TetherHostSource::DEVICE_SYNC_CLIENT;
  }
  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
      !base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    return TetherHostSource::REMOTE_DEVICE_PROVIDER;
  }
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
      base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    return IsInLegacyHostMode() ? TetherHostSource::DEVICE_SYNC_CLIENT
                                : TetherHostSource::MULTIDEVICE_SETUP_CLIENT;
  }
  NOTREACHED() << "TetherHostFetcherImpl: Unexpected feature flag state of "
               << "kMultiDeviceApi disabled and kEnableUnifiedMultiDeviceSetup "
               << "enabled.";
  return TetherHostSource::UNKNOWN;
}

}  // namespace tether

}  // namespace chromeos
