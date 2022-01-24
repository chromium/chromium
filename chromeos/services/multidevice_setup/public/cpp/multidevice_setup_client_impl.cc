// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// static
MultiDeviceSetupClientImpl::Factory*
    MultiDeviceSetupClientImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<MultiDeviceSetupClient>
MultiDeviceSetupClientImpl::Factory::Create(
    mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup) {
  if (test_factory_)
    return test_factory_->CreateInstance(std::move(remote_setup));

  return base::WrapUnique(
      new MultiDeviceSetupClientImpl(std::move(remote_setup)));
}

// static
void MultiDeviceSetupClientImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupClientImpl::Factory::~Factory() = default;

MultiDeviceSetupClientImpl::MultiDeviceSetupClientImpl(
    mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup)
    : multidevice_setup_remote_(std::move(remote_setup)),
      remote_device_cache_(multidevice::RemoteDeviceCache::Factory::Create()),
      host_status_with_device_(GenerateDefaultHostStatusWithDevice()),
      feature_states_map_(GenerateDefaultFeatureStatesMap(
          mojom::FeatureState::kUnavailableNoVerifiedHost_ClientNotReady)) {
  multidevice_setup_remote_->AddHostStatusObserver(
      GenerateHostStatusObserverRemote());
  multidevice_setup_remote_->AddFeatureStateObserver(
      GenerateFeatureStatesObserverRemote());
  multidevice_setup_remote_->GetHostStatus(
      base::BindOnce(&MultiDeviceSetupClientImpl::OnHostStatusChanged,
                     base::Unretained(this)));
  multidevice_setup_remote_->GetFeatureStates(
      base::BindOnce(&MultiDeviceSetupClientImpl::OnFeatureStatesChanged,
                     base::Unretained(this)));
}

MultiDeviceSetupClientImpl::~MultiDeviceSetupClientImpl() = default;

void MultiDeviceSetupClientImpl::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  multidevice_setup_remote_->GetEligibleHostDevices(base::BindOnce(
      &MultiDeviceSetupClientImpl::OnGetEligibleHostDevicesCompleted,
      base::Unretained(this), std::move(callback)));
}

void MultiDeviceSetupClientImpl::SetHostDevice(
    const std::string& host_instance_id_or_legacy_device_id,
    const std::string& auth_token,
    mojom::MultiDeviceSetup::SetHostDeviceCallback callback) {
  multidevice_setup_remote_->SetHostDevice(host_instance_id_or_legacy_device_id,
                                           auth_token, std::move(callback));
}

void MultiDeviceSetupClientImpl::RemoveHostDevice() {
  multidevice_setup_remote_->RemoveHostDevice();
}

const MultiDeviceSetupClient::HostStatusWithDevice&
MultiDeviceSetupClientImpl::GetHostStatus() const {
  return host_status_with_device_;
}

void MultiDeviceSetupClientImpl::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    const absl::optional<std::string>& auth_token,
    mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) {
  multidevice_setup_remote_->SetFeatureEnabledState(
      feature, enabled, auth_token, std::move(callback));
}

const MultiDeviceSetupClient::FeatureStatesMap&
MultiDeviceSetupClientImpl::GetFeatureStates() const {
  PA_LOG(VERBOSE)
      << "Responding to GetFeaturesStates() with the following cached map: "
      << FeatureStatesMapToString(feature_states_map_);
  return feature_states_map_;
}

void MultiDeviceSetupClientImpl::RetrySetHostNow(
    mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) {
  multidevice_setup_remote_->RetrySetHostNow(std::move(callback));
}

void MultiDeviceSetupClientImpl::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) {
  multidevice_setup_remote_->TriggerEventForDebugging(type,
                                                      std::move(callback));
}

void MultiDeviceSetupClientImpl::OnHostStatusChanged(
    mojom::HostStatus host_status,
    const absl::optional<multidevice::RemoteDevice>& host_device) {
  if (host_device) {
    remote_device_cache_->SetRemoteDevices({*host_device});
    host_status_with_device_ = std::make_pair(
        host_status, remote_device_cache_->GetRemoteDevice(
                         host_device->instance_id, host_device->GetDeviceId()));
  } else {
    host_status_with_device_ =
        std::make_pair(host_status, absl::nullopt /* host_device */);
  }

  NotifyHostStatusChanged(host_status_with_device_);
}

void MultiDeviceSetupClientImpl::OnFeatureStatesChanged(
    const FeatureStatesMap& feature_states_map) {
  PA_LOG(INFO) << "Feature states have changed. New feature map: "
               << FeatureStatesMapToString(feature_states_map);
  feature_states_map_ = feature_states_map;
  NotifyFeatureStateChanged(feature_states_map_);
}

void MultiDeviceSetupClientImpl::OnGetEligibleHostDevicesCompleted(
    GetEligibleHostDevicesCallback callback,
    const multidevice::RemoteDeviceList& eligible_host_devices) {
  remote_device_cache_->SetRemoteDevices(eligible_host_devices);

  multidevice::RemoteDeviceRefList eligible_host_device_refs;
  std::transform(eligible_host_devices.begin(), eligible_host_devices.end(),
                 std::back_inserter(eligible_host_device_refs),
                 [this](const auto& device) {
                   return *remote_device_cache_->GetRemoteDevice(
                       device.instance_id, device.GetDeviceId());
                 });

  std::move(callback).Run(eligible_host_device_refs);
}

mojo::PendingRemote<mojom::HostStatusObserver>
MultiDeviceSetupClientImpl::GenerateHostStatusObserverRemote() {
  return host_status_observer_receiver_.BindNewPipeAndPassRemote();
}

mojo::PendingRemote<mojom::FeatureStateObserver>
MultiDeviceSetupClientImpl::GenerateFeatureStatesObserverRemote() {
  return feature_state_observer_receiver_.BindNewPipeAndPassRemote();
}

void MultiDeviceSetupClientImpl::FlushForTesting() {
  multidevice_setup_remote_.FlushForTesting();
}

}  // namespace multidevice_setup

}  // namespace chromeos
