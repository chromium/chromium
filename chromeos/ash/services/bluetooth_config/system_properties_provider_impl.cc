// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/system_properties_provider_impl.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash::bluetooth_config {

SystemPropertiesProviderImpl::SystemPropertiesProviderImpl(
    AdapterStateController* adapter_state_controller,
    DeviceCache* device_cache,
    FastPairDelegate* fast_pair_delegate)
    : adapter_state_controller_(adapter_state_controller),
      device_cache_(device_cache),
      fast_pair_delegate_(fast_pair_delegate) {
  adapter_state_controller_observation_.Observe(
      adapter_state_controller_.get());
  device_cache_observation_.Observe(device_cache_.get());
  session_manager::SessionManager::Get()->AddObserver(this);
  if (fast_pair_delegate_) {
    fast_pair_delegate_observation_.Observe(fast_pair_delegate_.get());
  }
}

SystemPropertiesProviderImpl::~SystemPropertiesProviderImpl() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();

  // |session_manager| is null when we are shutting down and this class is being
  // destroyed because there is no longer a session.
  if (session_manager)
    session_manager->RemoveObserver(this);
}

void SystemPropertiesProviderImpl::OnAdapterStateChanged() {
  NotifyPropertiesChanged();
}

void SystemPropertiesProviderImpl::OnSessionStateChanged() {
  TRACE_EVENT0("login", "SystemPropertiesProviderImpl::OnSessionStateChanged");
  NotifyPropertiesChanged();
}

void SystemPropertiesProviderImpl::OnPairedDevicesListChanged() {
  NotifyPropertiesChanged();
}

void SystemPropertiesProviderImpl::OnFastPairableDevicesChanged(
    const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
        fast_pairable_devices) {
  NotifyPropertiesChanged();
}

mojom::BluetoothSystemState SystemPropertiesProviderImpl::ComputeSystemState()
    const {
  return adapter_state_controller_->GetAdapterState();
}

std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
SystemPropertiesProviderImpl::GetPairedDevices() const {
  return device_cache_->GetPairedDevices();
}

std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
SystemPropertiesProviderImpl::GetFastPairableDevices() const {
  if (fast_pair_delegate_) {
    return fast_pair_delegate_->GetFastPairableDeviceProperties();
  } else {
    return std::vector<mojom::PairedBluetoothDevicePropertiesPtr>();
  }
}

mojom::BluetoothModificationState
SystemPropertiesProviderImpl::ComputeModificationState() const {
  // Bluetooth power setting is always mutable in login screen before any
  // user logs in. The changes will affect local state preferences.
  //
  // Otherwise, the bluetooth setting should be mutable only if:
  // * the active user is the primary user, and
  // * the session is not in lock screen
  // The changes will affect the primary user's preferences.
  if (!session_manager::SessionManager::Get()->IsSessionStarted())
    return mojom::BluetoothModificationState::kCanModifyBluetooth;

  if (session_manager::SessionManager::Get()->IsScreenLocked())
    return mojom::BluetoothModificationState::kCannotModifyBluetooth;

  return user_manager::UserManager::Get()->GetPrimaryUser() ==
                 user_manager::UserManager::Get()->GetActiveUser()
             ? mojom::BluetoothModificationState::kCanModifyBluetooth
             : mojom::BluetoothModificationState::kCannotModifyBluetooth;
}

}  // namespace ash::bluetooth_config
