// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/discovery_session_manager_impl.h"

#include "chromeos/services/bluetooth_config/device_pairing_handler_impl.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

const char kDiscoveryClientName[] = "CrosBluetoothConfig API";

}  // namespace

DiscoverySessionManagerImpl::DiscoverySessionManagerImpl(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DiscoveredDevicesProvider* discovered_devices_provider)
    : DiscoverySessionManager(adapter_state_controller,
                              discovered_devices_provider),
      bluetooth_adapter_(std::move(bluetooth_adapter)) {
  adapter_observation_.Observe(bluetooth_adapter_.get());
}

DiscoverySessionManagerImpl::~DiscoverySessionManagerImpl() = default;

bool DiscoverySessionManagerImpl::IsDiscoverySessionActive() const {
  return discovery_session_ != nullptr;
}

void DiscoverySessionManagerImpl::OnHasAtLeastOneDiscoveryClientChanged() {
  UpdateDiscoveryState();
}

std::unique_ptr<DevicePairingHandler>
DiscoverySessionManagerImpl::CreateDevicePairingHandler(
    AdapterStateController* adapter_state_controller,
    mojo::PendingReceiver<mojom::DevicePairingHandler> receiver,
    base::OnceClosure finished_pairing_callback) {
  return DevicePairingHandlerImpl::Factory::Create(
      std::move(receiver), adapter_state_controller, bluetooth_adapter_,
      std::move(finished_pairing_callback));
}

void DiscoverySessionManagerImpl::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  // We only need to handle the case where we have an active discovery session
  // which stopped unexpectedly.
  if (discovering || !discovery_session_)
    return;

  // |discovery_session_| is no longer operational, so destroy it.
  DestroyDiscoverySession();
}

void DiscoverySessionManagerImpl::UpdateDiscoveryState() {
  // At least one client requests discovery, but it is not active yet, so
  // start a discovery attempt.
  if (HasAtLeastOneDiscoveryClient() && !discovery_session_) {
    AttemptDiscovery();
    return;
  }

  // No clients remain, but discovery is active; destroy the current session.
  if (!HasAtLeastOneDiscoveryClient() && discovery_session_)
    DestroyDiscoverySession();
}

void DiscoverySessionManagerImpl::AttemptDiscovery() {
  if (is_discovery_attempt_in_progress_)
    return;

  is_discovery_attempt_in_progress_ = true;
  bluetooth_adapter_->StartDiscoverySession(
      kDiscoveryClientName,
      base::BindOnce(&DiscoverySessionManagerImpl::OnDiscoverySuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DiscoverySessionManagerImpl::OnDiscoveryError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DiscoverySessionManagerImpl::OnDiscoverySuccess(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  is_discovery_attempt_in_progress_ = false;
  discovery_session_ = std::move(discovery_session);
  NotifyDiscoveryStarted();

  // Immediately inform the client of any devices already present once discovery
  // started.
  NotifyDiscoveredDevicesListChanged();

  // Discovery could have been cancelled between when StartDiscoverySession()
  // was called and when the callback was invoked. Check to see whether the new
  // discovery session should be deleted.
  UpdateDiscoveryState();
}

void DiscoverySessionManagerImpl::OnDiscoveryError() {
  is_discovery_attempt_in_progress_ = false;

  // Retry discovery. Note that we choose not to set a limit on the number of
  // retries because this operation only occurs while the pairing UI is open.
  UpdateDiscoveryState();
}

void DiscoverySessionManagerImpl::DestroyDiscoverySession() {
  discovery_session_.reset();
  NotifyDiscoveryStoppedAndClearActiveClients();
}

}  // namespace bluetooth_config
}  // namespace chromeos
