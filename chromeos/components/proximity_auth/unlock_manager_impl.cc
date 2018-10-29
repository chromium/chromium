// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/unlock_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/components/proximity_auth/metrics.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_monitor_impl.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/secure_context.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

using chromeos::DBusThreadManager;

namespace proximity_auth {
namespace {

// The maximum amount of time, in seconds, that the unlock manager can stay in
// the 'waking up' state after resuming from sleep.
const int kWakingUpDurationSecs = 15;

// The limit, in seconds, on the elapsed time for an auth attempt. If an auth
// attempt exceeds this limit, it will time out and be rejected. This is
// provided as a failsafe, in case something goes wrong.
const int kAuthAttemptTimeoutSecs = 5;

// Returns the remote device's security settings state, for metrics,
// corresponding to a remote status update.
metrics::RemoteSecuritySettingsState GetRemoteSecuritySettingsState(
    const RemoteStatusUpdate& status_update) {
  switch (status_update.secure_screen_lock_state) {
    case SECURE_SCREEN_LOCK_STATE_UNKNOWN:
      return metrics::RemoteSecuritySettingsState::UNKNOWN;

    case SECURE_SCREEN_LOCK_DISABLED:
      switch (status_update.trust_agent_state) {
        case TRUST_AGENT_UNSUPPORTED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_UNSUPPORTED;
        case TRUST_AGENT_DISABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_DISABLED;
        case TRUST_AGENT_ENABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_ENABLED;
      }

    case SECURE_SCREEN_LOCK_ENABLED:
      switch (status_update.trust_agent_state) {
        case TRUST_AGENT_UNSUPPORTED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_UNSUPPORTED;
        case TRUST_AGENT_DISABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_DISABLED;
        case TRUST_AGENT_ENABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_ENABLED;
      }
  }

  NOTREACHED();
  return metrics::RemoteSecuritySettingsState::UNKNOWN;
}

}  // namespace

class ProximityAuthPrefManager;

UnlockManagerImpl::UnlockManagerImpl(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    ProximityAuthClient* proximity_auth_client,
    ProximityAuthPrefManager* pref_manager)
    : screenlock_type_(screenlock_type),
      life_cycle_(nullptr),
      proximity_auth_client_(proximity_auth_client),
      pref_manager_(pref_manager),
      is_locked_(false),
      is_attempting_auth_(false),
      is_waking_up_(false),
      screenlock_state_(ScreenlockState::INACTIVE),
      clear_waking_up_state_weak_ptr_factory_(this),
      reject_auth_attempt_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  ScreenlockBridge* screenlock_bridge = ScreenlockBridge::Get();
  screenlock_bridge->AddObserver(this);
  OnScreenLockedOrUnlocked(screenlock_bridge->IsLocked());

  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);

  SetWakingUpState(true /* is_waking_up */);

  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&UnlockManagerImpl::OnBluetoothAdapterInitialized,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

UnlockManagerImpl::~UnlockManagerImpl() {
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);

  ScreenlockBridge::Get()->RemoveObserver(this);

  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);

  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

bool UnlockManagerImpl::IsUnlockAllowed() {
  return (remote_screenlock_state_ &&
          *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED &&
          life_cycle_ &&
          life_cycle_->GetState() ==
              RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED &&
          proximity_monitor_ && proximity_monitor_->IsUnlockAllowed() &&
          (screenlock_type_ != ProximityAuthSystem::SIGN_IN ||
           (GetMessenger() && GetMessenger()->SupportsSignIn())));
}

void UnlockManagerImpl::SetRemoteDeviceLifeCycle(
    RemoteDeviceLifeCycle* life_cycle) {
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);

  life_cycle_ = life_cycle;
  if (life_cycle_) {
    AttemptToStartRemoteDeviceLifecycle();
    SetWakingUpState(true /* is_waking_up */);
  } else {
    proximity_monitor_.reset();
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::OnLifeCycleStateChanged() {
  RemoteDeviceLifeCycle::State state = life_cycle_->GetState();

  remote_screenlock_state_.reset();
  if (state == RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    DCHECK(life_cycle_->GetConnection() || life_cycle_->GetChannel());
    DCHECK(GetMessenger());
    proximity_monitor_ = CreateProximityMonitor(life_cycle_, pref_manager_);
    GetMessenger()->AddObserver(this);
  }

  if (state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED)
    SetWakingUpState(false /* is_waking_up */);

  UpdateLockScreen();
}

void UnlockManagerImpl::OnUnlockEventSent(bool success) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Sent easy_unlock event, but no auth attempted.";
    return;
  }

  AcceptAuthAttempt(success);
}

void UnlockManagerImpl::OnRemoteStatusUpdate(
    const RemoteStatusUpdate& status_update) {
  PA_LOG(INFO) << "Status Update: ("
               << "user_present=" << status_update.user_presence << ", "
               << "secure_screen_lock="
               << status_update.secure_screen_lock_state << ", "
               << "trust_agent=" << status_update.trust_agent_state << ")";
  metrics::RecordRemoteSecuritySettingsState(
      GetRemoteSecuritySettingsState(status_update));

  remote_screenlock_state_.reset(new RemoteScreenlockState(
      GetScreenlockStateFromRemoteUpdate(status_update)));

  // This also calls |UpdateLockScreen()|
  SetWakingUpState(false /* is_waking_up */);
}

void UnlockManagerImpl::OnDecryptResponse(const std::string& decrypted_bytes) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Decrypt response received but not attempting auth.";
    return;
  }

  if (decrypted_bytes.empty()) {
    PA_LOG(WARNING) << "Failed to decrypt sign-in challenge.";
    AcceptAuthAttempt(false);
  } else {
    sign_in_secret_.reset(new std::string(decrypted_bytes));
    if (GetMessenger())
      GetMessenger()->DispatchUnlockEvent();
  }
}

void UnlockManagerImpl::OnUnlockResponse(bool success) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Unlock response received but not attempting auth.";
    return;
  }

  PA_LOG(INFO) << "Unlock response from remote device: "
               << (success ? "success" : "failure");
  if (success && GetMessenger())
    GetMessenger()->DispatchUnlockEvent();
  else
    AcceptAuthAttempt(false);
}

void UnlockManagerImpl::OnDisconnected() {
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);
}

void UnlockManagerImpl::OnProximityStateChanged() {
  PA_LOG(INFO) << "Proximity state changed.";
  UpdateLockScreen();
}

void UnlockManagerImpl::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  OnScreenLockedOrUnlocked(true);
}

void UnlockManagerImpl::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  OnScreenLockedOrUnlocked(false);
}

void UnlockManagerImpl::OnFocusedUserChanged(const AccountId& account_id) {}

void UnlockManagerImpl::OnScreenLockedOrUnlocked(bool is_locked) {
  if (is_locked && IsBluetoothPresentAndPowered() && life_cycle_)
    SetWakingUpState(true /* is_waking_up */);

  is_locked_ = is_locked;
  UpdateProximityMonitorState();
}

void UnlockManagerImpl::OnBluetoothAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);
}

void UnlockManagerImpl::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                              bool present) {
  UpdateLockScreen();
}

void UnlockManagerImpl::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                              bool powered) {
  UpdateLockScreen();
}

void UnlockManagerImpl::SuspendDone(const base::TimeDelta& sleep_duration) {
  SetWakingUpState(true /* is_waking_up */);
}

bool UnlockManagerImpl::IsBluetoothPresentAndPowered() const {
  return bluetooth_adapter_ && bluetooth_adapter_->IsPresent() &&
         bluetooth_adapter_->IsPowered();
}

void UnlockManagerImpl::AttemptToStartRemoteDeviceLifecycle() {
  if (IsBluetoothPresentAndPowered() && life_cycle_ &&
      life_cycle_->GetState() == RemoteDeviceLifeCycle::State::STOPPED) {
    // If Bluetooth is disabled after this, |life_cycle_| will be notified by
    // SecureChannel that the connection attempt failed. From that point on,
    // |life_cycle_| will wait to be started again by UnlockManager.
    life_cycle_->Start();
  }
}

void UnlockManagerImpl::OnAuthAttempted(mojom::AuthType auth_type) {
  if (is_attempting_auth_) {
    PA_LOG(INFO) << "Already attempting auth.";
    return;
  }

  if (auth_type != mojom::AuthType::USER_CLICK)
    return;

  is_attempting_auth_ = true;

  if (!life_cycle_ || !GetMessenger()) {
    PA_LOG(ERROR) << "No life_cycle active when auth is attempted";
    AcceptAuthAttempt(false);
    UpdateLockScreen();
    return;
  }

  if (!IsUnlockAllowed()) {
    AcceptAuthAttempt(false);
    UpdateLockScreen();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UnlockManagerImpl::AcceptAuthAttempt,
                     reject_auth_attempt_weak_ptr_factory_.GetWeakPtr(), false),
      base::TimeDelta::FromSeconds(kAuthAttemptTimeoutSecs));

  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN) {
    SendSignInChallenge();
  } else {
    if (GetMessenger()->SupportsSignIn()) {
      GetMessenger()->RequestUnlock();
    } else {
      PA_LOG(INFO) << "Protocol v3.1 not supported, skipping request_unlock.";
      GetMessenger()->DispatchUnlockEvent();
    }
  }
}

std::unique_ptr<ProximityMonitor> UnlockManagerImpl::CreateProximityMonitor(
    RemoteDeviceLifeCycle* life_cycle,
    ProximityAuthPrefManager* pref_manager) {
  return std::make_unique<ProximityMonitorImpl>(
      life_cycle->GetRemoteDevice(),
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
          ? life_cycle->GetChannel()
          : nullptr,
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
          ? nullptr
          : life_cycle->GetConnection(),
      pref_manager);
}

void UnlockManagerImpl::SendSignInChallenge() {
  if (!life_cycle_ || !GetMessenger()) {
    PA_LOG(ERROR) << "Not ready to send sign-in challenge";
    return;
  }

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    if (!GetMessenger()->GetChannel()) {
      PA_LOG(ERROR) << "Channel is not ready to send sign-in challenge.";
      return;
    }

    GetMessenger()->GetChannel()->GetConnectionMetadata(
        base::BindOnce(&UnlockManagerImpl::OnGetConnectionMetadata,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    if (!GetMessenger()->GetSecureContext()) {
      PA_LOG(ERROR) << "SecureContext is not ready to send sign-in challenge.";
      return;
    }

    cryptauth::RemoteDeviceRef remote_device = life_cycle_->GetRemoteDevice();
    proximity_auth_client_->GetChallengeForUserAndDevice(
        remote_device.user_id(), remote_device.public_key(),
        GetMessenger()->GetSecureContext()->GetChannelBindingData(),
        base::Bind(&UnlockManagerImpl::OnGotSignInChallenge,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void UnlockManagerImpl::OnGetConnectionMetadata(
    chromeos::secure_channel::mojom::ConnectionMetadataPtr
        connection_metadata_ptr) {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  cryptauth::RemoteDeviceRef remote_device = life_cycle_->GetRemoteDevice();
  proximity_auth_client_->GetChallengeForUserAndDevice(
      remote_device.user_id(), remote_device.public_key(),
      connection_metadata_ptr->channel_binding_data,
      base::Bind(&UnlockManagerImpl::OnGotSignInChallenge,
                 weak_ptr_factory_.GetWeakPtr()));
}

void UnlockManagerImpl::OnGotSignInChallenge(const std::string& challenge) {
  PA_LOG(INFO) << "Got sign-in challenge, sending for decryption...";
  if (GetMessenger())
    GetMessenger()->RequestDecryption(challenge);
}

ScreenlockState UnlockManagerImpl::GetScreenlockState() {
  if (!life_cycle_)
    return ScreenlockState::INACTIVE;

  if (!IsBluetoothPresentAndPowered())
    return ScreenlockState::NO_BLUETOOTH;

  if (IsUnlockAllowed())
    return ScreenlockState::AUTHENTICATED;

  RemoteDeviceLifeCycle::State life_cycle_state = life_cycle_->GetState();
  if (life_cycle_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED)
    return ScreenlockState::PHONE_NOT_AUTHENTICATED;

  if (is_waking_up_)
    return ScreenlockState::BLUETOOTH_CONNECTING;

  Messenger* messenger = GetMessenger();

  // Show a timeout state if we can not connect to the remote device in a
  // reasonable amount of time.
  if (!is_waking_up_ && !messenger)
    return ScreenlockState::NO_PHONE;

  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN && messenger &&
      !messenger->SupportsSignIn())
    return ScreenlockState::PHONE_UNSUPPORTED;

  // If the RSSI is too low, then the remote device is nowhere near the local
  // device. This message should take priority over messages about screen lock
  // states.
  if (!proximity_monitor_->IsUnlockAllowed()) {
    if (remote_screenlock_state_ &&
        *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED) {
      return ScreenlockState::RSSI_TOO_LOW;
    } else {
      return ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW;
    }
  }

  if (remote_screenlock_state_) {
    switch (*remote_screenlock_state_) {
      case RemoteScreenlockState::DISABLED:
        return ScreenlockState::PHONE_NOT_LOCKABLE;

      case RemoteScreenlockState::LOCKED:
        return ScreenlockState::PHONE_LOCKED;

      case RemoteScreenlockState::UNKNOWN:
        return ScreenlockState::PHONE_UNSUPPORTED;

      case RemoteScreenlockState::UNLOCKED:
        // Handled by the code below.
        break;
    }
  }

  return ScreenlockState::NO_PHONE;
}

void UnlockManagerImpl::UpdateLockScreen() {
  AttemptToStartRemoteDeviceLifecycle();

  UpdateProximityMonitorState();

  ScreenlockState new_state = GetScreenlockState();
  if (screenlock_state_ == new_state)
    return;

  PA_LOG(INFO) << "Updating screenlock state from " << screenlock_state_
               << " to " << new_state;
  proximity_auth_client_->UpdateScreenlockState(new_state);
  screenlock_state_ = new_state;
}

void UnlockManagerImpl::UpdateProximityMonitorState() {
  if (!proximity_monitor_)
    return;

  if (is_locked_ && life_cycle_ &&
      life_cycle_->GetState() ==
          RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    proximity_monitor_->AddObserver(this);
    proximity_monitor_->Start();
  } else {
    proximity_monitor_->Stop();
  }
}

void UnlockManagerImpl::SetWakingUpState(bool is_waking_up) {
  is_waking_up_ = is_waking_up;

  // Clear the waking up state after a timeout.
  clear_waking_up_state_weak_ptr_factory_.InvalidateWeakPtrs();
  if (is_waking_up_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UnlockManagerImpl::SetWakingUpState,
                       clear_waking_up_state_weak_ptr_factory_.GetWeakPtr(),
                       false),
        base::TimeDelta::FromSeconds(kWakingUpDurationSecs));
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::AcceptAuthAttempt(bool should_accept) {
  if (!is_attempting_auth_)
    return;

  // Cancel the pending task to time out the auth attempt.
  reject_auth_attempt_weak_ptr_factory_.InvalidateWeakPtrs();

  if (should_accept)
    proximity_monitor_->RecordProximityMetricsOnAuthSuccess();

  is_attempting_auth_ = false;
  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN) {
    PA_LOG(INFO) << "Finalizing sign-in...";
    proximity_auth_client_->FinalizeSignin(
        should_accept && sign_in_secret_ ? *sign_in_secret_ : std::string());
  } else {
    PA_LOG(INFO) << "Finalizing unlock...";
    proximity_auth_client_->FinalizeUnlock(should_accept);
  }
}

UnlockManagerImpl::RemoteScreenlockState
UnlockManagerImpl::GetScreenlockStateFromRemoteUpdate(
    RemoteStatusUpdate update) {
  switch (update.secure_screen_lock_state) {
    case SECURE_SCREEN_LOCK_DISABLED:
      return RemoteScreenlockState::DISABLED;

    case SECURE_SCREEN_LOCK_ENABLED:
      if (update.user_presence == USER_PRESENT)
        return RemoteScreenlockState::UNLOCKED;

      return RemoteScreenlockState::LOCKED;

    case SECURE_SCREEN_LOCK_STATE_UNKNOWN:
      return RemoteScreenlockState::UNKNOWN;
  }

  NOTREACHED();
  return RemoteScreenlockState::UNKNOWN;
}

Messenger* UnlockManagerImpl::GetMessenger() {
  // TODO(tengs): We should use a weak pointer to hold the Messenger instance
  // instead.
  if (!life_cycle_)
    return nullptr;
  return life_cycle_->GetMessenger();
}

}  // namespace proximity_auth
