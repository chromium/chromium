// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/proximity_auth_system.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/ash/components/proximity_auth/remote_device_life_cycle_impl.h"
#include "chromeos/ash/components/proximity_auth/unlock_manager_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace proximity_auth {

ProximityAuthSystem::ProximityAuthSystem(
    ProximityAuthClient* proximity_auth_client,
    ash::secure_channel::SecureChannelClient* secure_channel_client)
    : secure_channel_client_(secure_channel_client),
      unlock_manager_(
          std::make_unique<UnlockManagerImpl>(proximity_auth_client)) {}

ProximityAuthSystem::ProximityAuthSystem(
    ash::secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<UnlockManager> unlock_manager)
    : secure_channel_client_(secure_channel_client),
      unlock_manager_(std::move(unlock_manager)) {}

ProximityAuthSystem::~ProximityAuthSystem() {
  ScreenlockBridge::Get()->RemoveObserver(this);
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

void ProximityAuthSystem::Start() {
  if (started_)
    return;
  started_ = true;
  ScreenlockBridge::Get()->AddObserver(this);
  const AccountId& focused_account_id =
      ScreenlockBridge::Get()->focused_account_id();
  if (focused_account_id.is_valid())
    OnFocusedUserChanged(focused_account_id);
}

void ProximityAuthSystem::Stop() {
  if (!started_)
    return;
  started_ = false;
  ScreenlockBridge::Get()->RemoveObserver(this);
  OnFocusedUserChanged(EmptyAccountId());
}

void ProximityAuthSystem::SetRemoteDevicesForUser(
    const AccountId& account_id,
    const ash::multidevice::RemoteDeviceRefList& remote_devices,
    std::optional<ash::multidevice::RemoteDeviceRef> local_device) {
  PA_LOG(VERBOSE) << "Setting devices for user " << account_id.Serialize()
                  << ". Remote device count: " << remote_devices.size()
                  << ", Local device: ["
                  << (local_device.has_value() ? "present" : "absent") << "].";

  remote_devices_map_[account_id] = remote_devices;
  if (local_device) {
    local_device_map_.emplace(account_id, *local_device);
  } else {
    local_device_map_.erase(account_id);
  }

  if (started_) {
    const AccountId& focused_account_id =
        ScreenlockBridge::Get()->focused_account_id();
    if (focused_account_id.is_valid())
      OnFocusedUserChanged(focused_account_id);
  }
}

ash::multidevice::RemoteDeviceRefList
ProximityAuthSystem::GetRemoteDevicesForUser(
    const AccountId& account_id) const {
  auto it = remote_devices_map_.find(account_id);
  if (it == remote_devices_map_.end()) {
    return ash::multidevice::RemoteDeviceRefList();
  }
  return it->second;
}

void ProximityAuthSystem::OnAuthAttempted() {
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

void ProximityAuthSystem::OnSuspend() {
  PA_LOG(INFO) << "Preparing for device suspension.";
  DCHECK(!suspended_);
  suspended_ = true;
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnSuspendDone() {
  PA_LOG(INFO) << "Device resumed from suspension.";
  DCHECK(suspended_);
  suspended_ = false;

  if (!ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(INFO) << "Suspend done, but no lock screen.";
  } else if (!started_) {
    PA_LOG(INFO) << "Suspend done, but not system started.";
  } else {
    OnFocusedUserChanged(ScreenlockBridge::Get()->focused_account_id());
  }
}

void ProximityAuthSystem::CancelConnectionAttempt() {
  unlock_manager_->CancelConnectionAttempt();
}

std::unique_ptr<RemoteDeviceLifeCycle>
ProximityAuthSystem::CreateRemoteDeviceLifeCycle(
    ash::multidevice::RemoteDeviceRef remote_device,
    std::optional<ash::multidevice::RemoteDeviceRef> local_device) {
  return std::make_unique<RemoteDeviceLifeCycleImpl>(
      remote_device, local_device, secure_channel_client_);
}

void ProximityAuthSystem::OnScreenDidLock() {
  const AccountId& focused_account_id =
      ScreenlockBridge::Get()->focused_account_id();
  if (focused_account_id.is_valid())
    OnFocusedUserChanged(focused_account_id);
}

void ProximityAuthSystem::OnScreenDidUnlock() {
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnFocusedUserChanged(const AccountId& account_id) {
  // Update the current RemoteDeviceLifeCycle to the focused user.
  if (remote_device_life_cycle_) {
    if (remote_device_life_cycle_->GetRemoteDevice().user_email() !=
        account_id.GetUserEmail()) {
      PA_LOG(INFO) << "Focused user changed, destroying life cycle for "
                   << account_id.Serialize() << ".";
      unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
      remote_device_life_cycle_.reset();
    } else {
      PA_LOG(INFO) << "Refocused on a user who is already focused.";
      return;
    }
  }

  auto remote_devices_it = remote_devices_map_.find(account_id);
  if (remote_devices_it == remote_devices_map_.end() ||
      remote_devices_it->second.empty()) {
    PA_LOG(INFO) << "User " << account_id.Serialize()
                 << " does not have a Smart Lock host device.";
    return;
  }
  auto local_device_it = local_device_map_.find(account_id);
  if (local_device_it == local_device_map_.end()) {
    PA_LOG(INFO) << "User " << account_id.Serialize()
                 << " does not have a local device.";
    return;
  }

  // TODO(tengs): We currently assume each user has only one RemoteDevice, so we
  // can simply take the first item in the list.
  ash::multidevice::RemoteDeviceRef remote_device =
      remote_devices_it->second[0];

  std::optional<ash::multidevice::RemoteDeviceRef> local_device;
  local_device = local_device_it->second;

  if (!suspended_) {
    PA_LOG(INFO) << "Creating RemoteDeviceLifeCycle for focused user: "
                 << account_id.Serialize();
    remote_device_life_cycle_ =
        CreateRemoteDeviceLifeCycle(remote_device, local_device);

    // UnlockManager listens for Bluetooth power change events, and is therefore
    // responsible for starting RemoteDeviceLifeCycle when Bluetooth becomes
    // powered.
    unlock_manager_->SetRemoteDeviceLifeCycle(remote_device_life_cycle_.get());
  }
}

std::string ProximityAuthSystem::GetLastRemoteStatusUnlockForLogging() {
  if (unlock_manager_) {
    return unlock_manager_->GetLastRemoteStatusUnlockForLogging();
  }
  return std::string();
}

}  // namespace proximity_auth
