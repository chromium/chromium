// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/adapter_state_controller_impl.h"

#include "base/functional/bind.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"

namespace ash::bluetooth_config {

AdapterStateControllerImpl::AdapterStateControllerImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(std::move(bluetooth_adapter)) {
  adapter_observation_.Observe(bluetooth_adapter_.get());
}

AdapterStateControllerImpl::~AdapterStateControllerImpl() = default;

mojom::BluetoothSystemState AdapterStateControllerImpl::GetAdapterState()
    const {
  if (!bluetooth_adapter_->IsPresent())
    return mojom::BluetoothSystemState::kUnavailable;

  if (bluetooth_adapter_->IsPowered()) {
    return in_progress_state_change_ == PowerStateChange::kDisable
               ? mojom::BluetoothSystemState::kDisabling
               : mojom::BluetoothSystemState::kEnabled;
  }

  return in_progress_state_change_ == PowerStateChange::kEnable
             ? mojom::BluetoothSystemState::kEnabling
             : mojom::BluetoothSystemState::kDisabled;
}

void AdapterStateControllerImpl::SetBluetoothEnabledState(bool enabled) {
  queued_state_change_ =
      enabled ? PowerStateChange::kEnable : PowerStateChange::kDisable;

  BLUETOOTH_LOG(EVENT) << "Setting queued Bluetooth state change to "
                       << queued_state_change_;
  AttemptQueuedStateChange();
}

void AdapterStateControllerImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  if (!present) {
    BLUETOOTH_LOG(EVENT)
        << "Adapter changed to not present; clearing state changes";
    in_progress_state_change_ = PowerStateChange::kNoChange;
    queued_state_change_ = PowerStateChange::kNoChange;
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  NotifyAdapterStateChanged();
}

void AdapterStateControllerImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  NotifyAdapterStateChanged();

  // No action is required if the adapter state changed without
  // AdapterStateController interaction.
  if (in_progress_state_change_ == PowerStateChange::kNoChange) {
    return;
  }

  // No action is required if the state change is unrelated to the in-progress
  // state change request.
  if (powered != (in_progress_state_change_ == PowerStateChange::kEnable)) {
    return;
  }

  // The adapter state changed to that requested by the in-progress state
  // change. Handle the change here rather than the SetPowered() success
  // callback to avoid timing issues (b/270447662).
  BLUETOOTH_LOG(EVENT) << "Bluetooth " << (powered ? "enabled" : "disabled")
                       << " successfully";
  in_progress_state_change_ = PowerStateChange::kNoChange;
  device::PoweredStateOperation power_operation =
      powered ? device::PoweredStateOperation::kEnable
              : device::PoweredStateOperation::kDisable;
  device::RecordPoweredStateOperationResult(power_operation, /*success=*/true);

  AttemptQueuedStateChange();
}

void AdapterStateControllerImpl::AttemptQueuedStateChange() {
  if (!bluetooth_adapter_->IsPresent()) {
    BLUETOOTH_LOG(EVENT) << "Adapter not present; clearing state changes";
    in_progress_state_change_ = PowerStateChange::kNoChange;
    queued_state_change_ = PowerStateChange::kNoChange;
    return;
  }

  // Cannot attempt to change state since the previous attempt is still in
  // progress.
  if (in_progress_state_change_ != PowerStateChange::kNoChange) {
    BLUETOOTH_LOG(EVENT)
        << "Not attempting to change state since the previous "
        << "change is still in progress. Continuing previous change: "
        << in_progress_state_change_;
    return;
  }

  switch (queued_state_change_) {
    // No queued change; return early.
    case PowerStateChange::kNoChange:
      return;

    case PowerStateChange::kEnable:
      AttemptSetEnabled(/*enabled=*/true);
      break;

    case PowerStateChange::kDisable:
      AttemptSetEnabled(/*enabled=*/false);
      break;
  }
}

void AdapterStateControllerImpl::AttemptSetEnabled(bool enabled) {
  DCHECK(bluetooth_adapter_->IsPresent());
  DCHECK_EQ(PowerStateChange::kNoChange, in_progress_state_change_);
  DCHECK_NE(PowerStateChange::kNoChange, queued_state_change_);

  // Already in the correct state; clear the queued change and return.
  if (bluetooth_adapter_->IsPowered() == enabled) {
    BLUETOOTH_LOG(EVENT) << "Already in state "
                         << (enabled ? "enabled" : "disabled")
                         << ", clearing queued state change";
    queued_state_change_ = PowerStateChange::kNoChange;
    return;
  }

  queued_state_change_ = PowerStateChange::kNoChange;
  in_progress_state_change_ =
      enabled ? PowerStateChange::kEnable : PowerStateChange::kDisable;

  BLUETOOTH_LOG(EVENT) << "Attempting to " << (enabled ? "enable" : "disable")
                       << " Bluetooth";
  bluetooth_adapter_->SetPowered(
      enabled,
      // Power state successfully updating is handled in AdapterPoweredChanged()
      // (see b/274973520).
      base::DoNothing(),
      base::BindOnce(&AdapterStateControllerImpl::OnSetPoweredError,
                     weak_ptr_factory_.GetWeakPtr(), enabled));
  device::RecordPoweredState(enabled);
  // State has changed to kEnabling or kDisabling; notify observers.
  NotifyAdapterStateChanged();
}

void AdapterStateControllerImpl::OnSetPoweredError(bool enabled) {
  BLUETOOTH_LOG(ERROR) << "Error attempting to "
                       << (enabled ? "enable" : "disable") << " Bluetooth";
  device::PoweredStateOperation power_operation =
      enabled ? device::PoweredStateOperation::kEnable
              : device::PoweredStateOperation::kDisable;
  device::RecordPoweredStateOperationResult(power_operation, /*success=*/false);
  in_progress_state_change_ = PowerStateChange::kNoChange;

  // State is no longer kEnabling or kDisabling; notify observers.
  NotifyAdapterStateChanged();

  AttemptQueuedStateChange();
}

std::ostream& operator<<(
    std::ostream& stream,
    const AdapterStateControllerImpl::PowerStateChange& power_state_change) {
  switch (power_state_change) {
    case AdapterStateControllerImpl::PowerStateChange::kNoChange:
      stream << "[No Change]";
      break;
    case AdapterStateControllerImpl::PowerStateChange::kEnable:
      stream << "[Enable]";
      break;
    case AdapterStateControllerImpl::PowerStateChange::kDisable:
      stream << "[Disable]";
      break;
  }
  return stream;
}

}  // namespace ash::bluetooth_config
