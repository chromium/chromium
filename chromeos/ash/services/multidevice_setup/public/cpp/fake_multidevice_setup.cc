// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

namespace multidevice_setup {

FakeMultiDeviceSetup::FakeMultiDeviceSetup() = default;

FakeMultiDeviceSetup::~FakeMultiDeviceSetup() {
  // Any callbacks which have not yet been invoked must be called here, since
  // Mojo invokes a crash when these callbacks are deleted without being called.
  for (auto& get_eligible_hosts_arg : get_eligible_hosts_args_) {
    if (get_eligible_hosts_arg)
      std::move(get_eligible_hosts_arg).Run(multidevice::RemoteDeviceList());
  }

  for (auto& get_eligible_active_hosts_arg : get_eligible_active_hosts_args_) {
    if (get_eligible_active_hosts_arg) {
      std::move(get_eligible_active_hosts_arg)
          .Run(std::vector<mojom::HostDevicePtr>());
    }
  }

  for (auto& set_host_arg : set_host_args_) {
    if (std::get<2>(set_host_arg))
      std::move(std::get<2>(set_host_arg)).Run(false /* success */);
  }

  for (auto& get_host_arg : get_host_args_) {
    if (get_host_arg) {
      std::move(get_host_arg)
          .Run(mojom::HostStatus::kNoEligibleHosts,
               std::nullopt /* host_device */);
    }
  }

  for (auto& set_feature_enabled_args : set_feature_enabled_args_) {
    if (std::get<3>(set_feature_enabled_args))
      std::move(std::get<3>(set_feature_enabled_args)).Run(false /* success */);
  }

  for (auto& get_feature_states_arg : get_feature_states_args_) {
    if (get_feature_states_arg) {
      std::move(get_feature_states_arg)
          .Run(base::flat_map<mojom::Feature, mojom::FeatureState>());
    }
  }

  for (auto& retry_set_host_now_arg : retry_set_host_now_args_) {
    if (retry_set_host_now_arg)
      std::move(retry_set_host_now_arg).Run(false /* success */);
  }

  for (auto& triggered_debug_event : triggered_debug_events_) {
    if (triggered_debug_event.second)
      std::move(triggered_debug_event.second).Run(false /* success */);
  }

  for (auto& get_qs_phone_instance_id_arg : get_qs_phone_instance_id_args_) {
    if (get_qs_phone_instance_id_arg) {
      std::move(get_qs_phone_instance_id_arg)
          .Run("" /* qs_phone_instance_id */);
    }
  }

  for (auto& set_host_without_auth_arg : set_host_without_auth_args_) {
    if (set_host_without_auth_arg.second)
      std::move(set_host_without_auth_arg.second).Run(false /* success */);
  }
}

void FakeMultiDeviceSetup::FlushForTesting() {
  host_status_observers_.FlushForTesting();
  feature_state_observers_.FlushForTesting();
}

bool FakeMultiDeviceSetup::HasAtLeastOneHostStatusObserver() {
  return !host_status_observers_.empty();
}

bool FakeMultiDeviceSetup::HasAtLeastOneFeatureStateObserver() {
  return !feature_state_observers_.empty();
}

void FakeMultiDeviceSetup::NotifyHostStatusChanged(
    mojom::HostStatus host_status,
    const std::optional<multidevice::RemoteDevice>& host_device) {
  for (auto& observer : host_status_observers_)
    observer->OnHostStatusChanged(host_status, host_device);
}

void FakeMultiDeviceSetup::NotifyFeatureStateChanged(
    const base::flat_map<mojom::Feature, mojom::FeatureState>&
        feature_states_map) {
  for (auto& observer : feature_state_observers_)
    observer->OnFeatureStatesChanged(feature_states_map);
}

void FakeMultiDeviceSetup::SetAccountStatusChangeDelegate(
    mojo::PendingRemote<mojom::AccountStatusChangeDelegate> delegate) {
  delegate_.reset();
  delegate_.Bind(std::move(delegate));
}

void FakeMultiDeviceSetup::AddHostStatusObserver(
    mojo::PendingRemote<mojom::HostStatusObserver> observer) {
  host_status_observers_.Add(std::move(observer));
}

void FakeMultiDeviceSetup::AddFeatureStateObserver(
    mojo::PendingRemote<mojom::FeatureStateObserver> observer) {
  feature_state_observers_.Add(std::move(observer));
}

void FakeMultiDeviceSetup::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  get_eligible_hosts_args_.push_back(std::move(callback));
}

void FakeMultiDeviceSetup::GetEligibleActiveHostDevices(
    GetEligibleActiveHostDevicesCallback callback) {
  get_eligible_active_hosts_args_.push_back(std::move(callback));
}

void FakeMultiDeviceSetup::SetHostDevice(
    const std::string& host_instance_id_or_legacy_device_id,
    const std::string& auth_token,
    SetHostDeviceCallback callback) {
  set_host_args_.emplace_back(host_instance_id_or_legacy_device_id, auth_token,
                              std::move(callback));
}

void FakeMultiDeviceSetup::RemoveHostDevice() {
  ++num_remove_host_calls_;
}

void FakeMultiDeviceSetup::GetHostStatus(GetHostStatusCallback callback) {
  get_host_args_.push_back(std::move(callback));
}

void FakeMultiDeviceSetup::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    const std::optional<std::string>& auth_token,
    SetFeatureEnabledStateCallback callback) {
  set_feature_enabled_args_.emplace_back(feature, enabled, auth_token,
                                         std::move(callback));
}

void FakeMultiDeviceSetup::GetFeatureStates(GetFeatureStatesCallback callback) {
  get_feature_states_args_.emplace_back(std::move(callback));
}

void FakeMultiDeviceSetup::RetrySetHostNow(RetrySetHostNowCallback callback) {
  retry_set_host_now_args_.push_back(std::move(callback));
}

void FakeMultiDeviceSetup::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  triggered_debug_events_.emplace_back(type, std::move(callback));
}

void FakeMultiDeviceSetup::SetQuickStartPhoneInstanceID(
    const std::string& qs_phone_instance_id) {
  set_qs_phone_instance_id_args_.emplace_back(qs_phone_instance_id);
}

void FakeMultiDeviceSetup::GetQuickStartPhoneInstanceID(
    GetQuickStartPhoneInstanceIDCallback callback) {
  get_qs_phone_instance_id_args_.emplace_back(std::move(callback));
}

void FakeMultiDeviceSetup::SetHostDeviceWithoutAuthToken(
    const std::string& host_instance_id_or_legacy_device_id,
    mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback) {
  set_host_without_auth_args_.emplace_back(host_instance_id_or_legacy_device_id,
                                           std::move(callback));
}

}  // namespace multidevice_setup

}  // namespace ash
