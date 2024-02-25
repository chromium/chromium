// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"

namespace ash {

namespace multidevice_setup {

FakeMultiDeviceSetupClient::FakeMultiDeviceSetupClient()
    : host_status_with_device_(GenerateDefaultHostStatusWithDevice()),
      feature_states_map_(GenerateDefaultFeatureStatesMap(
          mojom::FeatureState::kProhibitedByPolicy)) {}

FakeMultiDeviceSetupClient::~FakeMultiDeviceSetupClient() {
  DCHECK(get_eligible_host_devices_callback_queue_.empty());
  DCHECK(set_host_args_queue_.empty());
  DCHECK(set_feature_enabled_state_args_queue_.empty());
  DCHECK(retry_set_host_now_callback_queue_.empty());
  DCHECK(trigger_event_for_debugging_type_and_callback_queue_.empty());
}

void FakeMultiDeviceSetupClient::SetHostStatusWithDevice(
    const HostStatusWithDevice& host_status_with_device) {
  if (host_status_with_device == host_status_with_device_)
    return;

  host_status_with_device_ = host_status_with_device;
  MultiDeviceSetupClient::NotifyHostStatusChanged(host_status_with_device_);
}

void FakeMultiDeviceSetupClient::SetFeatureStates(
    const FeatureStatesMap& feature_states_map) {
  if (feature_states_map == feature_states_map_)
    return;

  feature_states_map_ = feature_states_map;
  MultiDeviceSetupClient::NotifyFeatureStateChanged(feature_states_map_);
}

void FakeMultiDeviceSetupClient::SetFeatureState(
    mojom::Feature feature,
    mojom::FeatureState feature_state) {
  if (feature_states_map_[feature] == feature_state)
    return;

  feature_states_map_[feature] = feature_state;
  MultiDeviceSetupClient::NotifyFeatureStateChanged(feature_states_map_);
}

void FakeMultiDeviceSetupClient::InvokePendingGetEligibleHostDevicesCallback(
    const multidevice::RemoteDeviceRefList& eligible_devices) {
  std::move(get_eligible_host_devices_callback_queue_.front())
      .Run(eligible_devices);
  get_eligible_host_devices_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingSetHostDeviceCallback(
    const std::string& expected_instance_id_or_legacy_device_id,
    const std::string& expected_auth_token,
    bool success) {
  DCHECK_EQ(expected_instance_id_or_legacy_device_id,
            std::get<0>(set_host_args_queue_.front()));
  DCHECK_EQ(expected_auth_token, std::get<1>(set_host_args_queue_.front()));
  std::move(std::get<2>(set_host_args_queue_.front())).Run(success);
  set_host_args_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingSetFeatureEnabledStateCallback(
    mojom::Feature expected_feature,
    bool expected_enabled,
    const std::optional<std::string>& expected_auth_token,
    bool success) {
  auto& tuple = set_feature_enabled_state_args_queue_.front();
  DCHECK_EQ(expected_feature, std::get<0>(tuple));
  DCHECK_EQ(expected_enabled, std::get<1>(tuple));
  DCHECK(expected_auth_token == std::get<2>(tuple));
  std::move(std::get<3>(tuple)).Run(success);
  set_feature_enabled_state_args_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingRetrySetHostNowCallback(
    bool success) {
  std::move(retry_set_host_now_callback_queue_.front()).Run(success);
  retry_set_host_now_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingTriggerEventForDebuggingCallback(
    mojom::EventTypeForDebugging expected_type,
    bool success) {
  DCHECK_EQ(expected_type,
            trigger_event_for_debugging_type_and_callback_queue_.front().first);
  std::move(trigger_event_for_debugging_type_and_callback_queue_.front().second)
      .Run(success);
  trigger_event_for_debugging_type_and_callback_queue_.pop();
}

size_t FakeMultiDeviceSetupClient::NumPendingSetFeatureEnabledStateCalls()
    const {
  return set_feature_enabled_state_args_queue_.size();
}

void FakeMultiDeviceSetupClient::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  get_eligible_host_devices_callback_queue_.push(std::move(callback));
}

void FakeMultiDeviceSetupClient::SetHostDevice(
    const std::string& host_instance_id_or_legacy_device_id,
    const std::string& auth_token,
    mojom::MultiDeviceSetup::SetHostDeviceCallback callback) {
  set_host_args_queue_.emplace(host_instance_id_or_legacy_device_id, auth_token,
                               std::move(callback));
}

void FakeMultiDeviceSetupClient::RemoveHostDevice() {
  num_remove_host_device_called_++;
}

const MultiDeviceSetupClient::HostStatusWithDevice&
FakeMultiDeviceSetupClient::GetHostStatus() const {
  return host_status_with_device_;
}

void FakeMultiDeviceSetupClient::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    const std::optional<std::string>& auth_token,
    mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) {
  set_feature_enabled_state_args_queue_.emplace(feature, enabled, auth_token,
                                                std::move(callback));
}

const MultiDeviceSetupClient::FeatureStatesMap&
FakeMultiDeviceSetupClient::GetFeatureStates() const {
  return feature_states_map_;
}

void FakeMultiDeviceSetupClient::RetrySetHostNow(
    mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) {
  retry_set_host_now_callback_queue_.push(std::move(callback));
}

void FakeMultiDeviceSetupClient::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) {
  trigger_event_for_debugging_type_and_callback_queue_.emplace(
      type, std::move(callback));
}

void FakeMultiDeviceSetupClient::SetQuickStartPhoneInstanceID(
    const std::string& qs_phone_instance_id) {
  qs_phone_instance_id_ = qs_phone_instance_id;
}

}  // namespace multidevice_setup

}  // namespace ash
