// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/unlock_manager_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/proximity_auth/messenger.h"
#include "chromeos/ash/components/proximity_auth/metrics.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/ash/components/proximity_auth/proximity_monitor_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace proximity_auth {
namespace {

using SmartLockState = ash::SmartLockState;

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other). Entries should be never modified
// or deleted. Only additions possible.
enum class FindAndConnectToHostResult {
  kFoundAndConnectedToHost = 0,
  kCanceledBluetoothDisabled = 1,
  kCanceledUserEnteredPassword = 2,
  kSecureChannelConnectionAttemptFailure = 3,
  kTimedOut = 4,
  kMaxValue = kTimedOut
};

// The maximum amount of time that the unlock manager can stay in the 'waking
// up' state after resuming from sleep.
constexpr base::TimeDelta kWakingUpDuration = base::Seconds(15);

// The maximum amount of time that we wait for the BluetoothAdapter to be
// fully initialized after resuming from sleep.
// TODO(crbug.com/986896): This is necessary because the BluetoothAdapter
// returns incorrect presence and power values directly after resume, and does
// not return correct values until about 1-2 seconds later. Remove this once
// the bug is fixed.
constexpr base::TimeDelta kBluetoothAdapterResumeMaxDuration = base::Seconds(3);

// The limit on the elapsed time for an auth attempt. If an auth attempt exceeds
// this limit, it will time out and be rejected. This is provided as a failsafe,
// in case something goes wrong.
constexpr base::TimeDelta kAuthAttemptTimeout = base::Seconds(5);

constexpr base::TimeDelta kMinExtendedDuration = base::Milliseconds(1);
constexpr base::TimeDelta kMaxExtendedDuration = base::Seconds(15);
const int kNumDurationMetricBuckets = 100;

const char kGetRemoteStatusNone[] = "none";
const char kGetRemoteStatusSuccess[] = "success";

// The subset of SmartLockStates that represent the first non-trival status
// shown to the user. Entries persisted to UMA histograms; do not reorder or
// delete enum values.
enum class FirstSmartLockStatus {
  kBluetoothDisabled = 0,
  kPhoneNotLockable = 1,
  kPhoneNotFound = 2,
  kPhoneNotAuthenticated = 3,
  kPhoneFoundLockedAndDistant = 4,
  kPhoneFoundLockedAndProximate = 5,
  kPhoneFoundUnlockedAndDistant = 6,
  kPhoneAuthenticated = 7,
  kPrimaryUserAbsent = 8,
  kMaxValue = kPrimaryUserAbsent
};

std::optional<FirstSmartLockStatus> GetFirstSmartLockStatus(
    SmartLockState state) {
  switch (state) {
    case SmartLockState::kBluetoothDisabled:
      return FirstSmartLockStatus::kBluetoothDisabled;
    case SmartLockState::kPhoneNotLockable:
      return FirstSmartLockStatus::kPhoneNotLockable;
    case SmartLockState::kPhoneNotFound:
      return FirstSmartLockStatus::kPhoneNotFound;
    case SmartLockState::kPhoneNotAuthenticated:
      return FirstSmartLockStatus::kPhoneNotAuthenticated;
    case SmartLockState::kPhoneFoundLockedAndDistant:
      return FirstSmartLockStatus::kPhoneFoundLockedAndDistant;
    case SmartLockState::kPhoneFoundLockedAndProximate:
      return FirstSmartLockStatus::kPhoneFoundLockedAndProximate;
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      return FirstSmartLockStatus::kPhoneFoundUnlockedAndDistant;
    case SmartLockState::kPhoneAuthenticated:
      return FirstSmartLockStatus::kPhoneAuthenticated;
    case SmartLockState::kPrimaryUserAbsent:
      return FirstSmartLockStatus::kPrimaryUserAbsent;
    default:
      return std::nullopt;
  }
}

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

  NOTREACHED_IN_MIGRATION();
  return metrics::RemoteSecuritySettingsState::UNKNOWN;
}

std::string GetHistogramStatusSuffix(bool unlockable) {
  return unlockable ? "Unlockable" : "Other";
}

void RecordFindAndConnectToHostResult(FindAndConnectToHostResult result) {
  base::UmaHistogramEnumeration("SmartLock.FindAndConnectToHostResult.Unlock",
                                result);
}

void RecordAuthResultFailure(
    SmartLockMetricsRecorder::SmartLockAuthResultFailureReason failure_reason) {
  SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(failure_reason);
}

void RecordExtendedDurationTimerMetric(const std::string& histogram_name,
                                       base::TimeDelta duration) {
  // Use a custom |max| to account for Smart Lock's timeout (larger than the
  // default 10 seconds).
  base::UmaHistogramCustomTimes(
      histogram_name, duration, kMinExtendedDuration /* min */,
      kMaxExtendedDuration /* max */, kNumDurationMetricBuckets /* buckets */);
}

bool HasCommunicatedWithPhone(SmartLockState state) {
  switch (state) {
    case SmartLockState::kDisabled:
      [[fallthrough]];
    case SmartLockState::kInactive:
      [[fallthrough]];
    case SmartLockState::kBluetoothDisabled:
      [[fallthrough]];
    case SmartLockState::kPhoneNotFound:
      [[fallthrough]];
    case SmartLockState::kConnectingToPhone:
      return false;
    case SmartLockState::kPhoneNotLockable:
      [[fallthrough]];
    case SmartLockState::kPhoneNotAuthenticated:
      [[fallthrough]];
    case SmartLockState::kPhoneFoundLockedAndDistant:
      [[fallthrough]];
    case SmartLockState::kPhoneFoundLockedAndProximate:
      [[fallthrough]];
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      [[fallthrough]];
    case SmartLockState::kPhoneAuthenticated:
      [[fallthrough]];
    case SmartLockState::kPrimaryUserAbsent:
      return true;
  }
}

}  // namespace

UnlockManagerImpl::UnlockManagerImpl(ProximityAuthClient* proximity_auth_client)
    : proximity_auth_client_(proximity_auth_client),
      bluetooth_suspension_recovery_timer_(
          std::make_unique<base::OneShotTimer>()) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::BindOnce(&UnlockManagerImpl::OnBluetoothAdapterInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

UnlockManagerImpl::~UnlockManagerImpl() {
  if (life_cycle_)
    life_cycle_->RemoveObserver(this);
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);
  if (proximity_monitor_)
    proximity_monitor_->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

bool UnlockManagerImpl::IsUnlockAllowed() {
  return (remote_screenlock_state_ &&
          *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED &&
          is_bluetooth_connection_to_phone_active_ && proximity_monitor_ &&
          proximity_monitor_->IsUnlockAllowed());
}

void UnlockManagerImpl::SetRemoteDeviceLifeCycle(
    RemoteDeviceLifeCycle* life_cycle) {
  PA_LOG(VERBOSE) << "Request received to change scan state to: "
                  << (life_cycle == nullptr ? "inactive" : "active") << ".";

  if (life_cycle_)
    life_cycle_->RemoveObserver(this);
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);

  life_cycle_ = life_cycle;
  if (life_cycle_) {
    life_cycle_->AddObserver(this);

    is_bluetooth_connection_to_phone_active_ = false;
    show_lock_screen_time_ = base::DefaultClock::GetInstance()->Now();
    has_user_been_shown_first_status_ = false;

    if (IsBluetoothPresentAndPowered()) {
      SetIsPerformingInitialScan(true /* is_performing_initial_scan */);
      AttemptToStartRemoteDeviceLifecycle();
    } else {
      RecordFindAndConnectToHostResult(
          FindAndConnectToHostResult::kCanceledBluetoothDisabled);
      SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    }
  } else {
    ResetPerformanceMetricsTimestamps();

    if (proximity_monitor_)
      proximity_monitor_->RemoveObserver(this);
    proximity_monitor_.reset();

    UpdateLockScreen();
  }
}

void UnlockManagerImpl::OnLifeCycleStateChanged(
    RemoteDeviceLifeCycle::State old_state,
    RemoteDeviceLifeCycle::State new_state) {
  remote_screenlock_state_.reset();
  if (new_state == RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    DCHECK(life_cycle_->GetChannel());
    DCHECK(GetMessenger());
    if (!proximity_monitor_) {
      proximity_monitor_ = CreateProximityMonitor(life_cycle_);
      proximity_monitor_->AddObserver(this);
      proximity_monitor_->Start();
    }
    GetMessenger()->AddObserver(this);

    is_bluetooth_connection_to_phone_active_ = true;
    attempt_get_remote_status_start_time_ =
        base::DefaultClock::GetInstance()->Now();

    PA_LOG(VERBOSE) << "Successfully connected to host; waiting for remote "
                       "status update.";

    if (is_performing_initial_scan_) {
      RecordFindAndConnectToHostResult(
          FindAndConnectToHostResult::kFoundAndConnectedToHost);
    }
  } else {
    is_bluetooth_connection_to_phone_active_ = false;

    if (proximity_monitor_) {
      proximity_monitor_->RemoveObserver(this);
      proximity_monitor_->Stop();
      proximity_monitor_.reset();
    }
  }

  // Note: though the name is AUTHENTICATION_FAILED, this state actually
  // encompasses any connection failure in
  // `ash::secure_channel::mojom::ConnectionAttemptFailureReason` beside
  // Bluetooth becoming disabled. See https://crbug.com/991644 for more.
  if (new_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED) {
    PA_LOG(ERROR) << "Connection attempt to host failed.";

    if (is_performing_initial_scan_) {
      RecordFindAndConnectToHostResult(
          FindAndConnectToHostResult::kSecureChannelConnectionAttemptFailure);
      SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    }
  }

  if (new_state == RemoteDeviceLifeCycle::State::FINDING_CONNECTION &&
      old_state == RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    PA_LOG(ERROR) << "Secure channel dropped for unknown reason; potentially "
                     "due to Bluetooth being disabled.";

    if (is_performing_initial_scan_) {
      OnDisconnected();
      SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    }
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::OnUnlockEventSent(bool success) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Sent easy_unlock event, but no auth attempted.";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockEventSentButNotAttemptingAuth);
  } else if (success) {
    FinalizeAuthAttempt(std::nullopt /* failure_reason */);
  } else {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedtoNotifyHostDeviceThatSmartLockWasUsed);
  }
}

void UnlockManagerImpl::OnRemoteStatusUpdate(
    const RemoteStatusUpdate& status_update) {
  PA_LOG(VERBOSE) << "Status Update: ("
                  << "user_present=" << status_update.user_presence << ", "
                  << "secure_screen_lock="
                  << status_update.secure_screen_lock_state << ", "
                  << "trust_agent=" << status_update.trust_agent_state << ")";
  metrics::RecordRemoteSecuritySettingsState(
      GetRemoteSecuritySettingsState(status_update));

  remote_screenlock_state_ = std::make_unique<RemoteScreenlockState>(
      GetScreenlockStateFromRemoteUpdate(status_update));

  // Only record these metrics within the initial period of opening the laptop
  // displaying the lock screen.
  if (is_performing_initial_scan_) {
    RecordFirstRemoteStatusReceived(
        *remote_screenlock_state_ ==
        RemoteScreenlockState::UNLOCKED /* unlockable */);
  }

  // This also calls |UpdateLockScreen()|
  SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
}

void UnlockManagerImpl::OnUnlockResponse(bool success) {
  if (!is_attempting_auth_) {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockRequestSentButNotAttemptingAuth);
    PA_LOG(ERROR) << "Unlock request sent but not attempting auth.";
    return;
  }

  PA_LOG(INFO) << "Received unlock response from device: "
               << (success ? "yes" : "no") << ".";

  if (success && GetMessenger()) {
    GetMessenger()->DispatchUnlockEvent();
  } else {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedToSendUnlockRequest);
  }
}

void UnlockManagerImpl::OnDisconnected() {
  if (is_attempting_auth_) {
    RecordAuthResultFailure(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kAuthenticatedChannelDropped);
  } else if (is_performing_initial_scan_) {
    RecordGetRemoteStatusResultFailure(
        GetRemoteStatusResultFailureReason::kAuthenticatedChannelDropped);
  }

  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);
}

void UnlockManagerImpl::OnProximityStateChanged() {
  PA_LOG(VERBOSE) << "Proximity state changed.";
  UpdateLockScreen();
}

void UnlockManagerImpl::OnBluetoothAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);
}

void UnlockManagerImpl::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                              bool present) {
  if (!IsBluetoothAdapterRecoveringFromSuspend())
    OnBluetoothAdapterPresentAndPoweredChanged();
}

void UnlockManagerImpl::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                              bool powered) {
  if (!IsBluetoothAdapterRecoveringFromSuspend())
    OnBluetoothAdapterPresentAndPoweredChanged();
}

void UnlockManagerImpl::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // TODO(crbug.com/986896): For a short time window after resuming from
  // suspension, BluetoothAdapter returns incorrect presence and power values.
  // Cache the correct values now, in case we need to check those values during
  // that time window when the device resumes.
  was_bluetooth_present_and_powered_before_last_suspend_ =
      IsBluetoothPresentAndPowered();
  bluetooth_suspension_recovery_timer_->Stop();
}

void UnlockManagerImpl::SuspendDone(base::TimeDelta sleep_duration) {
  bluetooth_suspension_recovery_timer_->Start(
      FROM_HERE, kBluetoothAdapterResumeMaxDuration,
      base::BindOnce(
          &UnlockManagerImpl::OnBluetoothAdapterPresentAndPoweredChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // The next scan after resuming is expected to be triggered by calling
  // SetRemoteDeviceLifeCycle().
}

bool UnlockManagerImpl::IsBluetoothPresentAndPowered() const {
  // TODO(crbug.com/986896): If the BluetoothAdapter is still "resuming after
  // suspension" at this time, it's prone to this bug, meaning we cannot trust
  // its returned presence and power values. If this is the case, depend on
  // the cached |was_bluetooth_present_and_powered_before_last_suspend_| to
  // signal if Bluetooth is enabled; otherwise, directly check request values
  // from BluetoothAdapter. Remove this check once the bug is fixed.
  if (IsBluetoothAdapterRecoveringFromSuspend())
    return was_bluetooth_present_and_powered_before_last_suspend_;

  return bluetooth_adapter_ && bluetooth_adapter_->IsPresent() &&
         bluetooth_adapter_->IsPowered();
}

void UnlockManagerImpl::OnBluetoothAdapterPresentAndPoweredChanged() {
  DCHECK(!IsBluetoothAdapterRecoveringFromSuspend());

  if (IsBluetoothPresentAndPowered()) {
    if (!is_performing_initial_scan_ &&
        !is_bluetooth_connection_to_phone_active_) {
      SetIsPerformingInitialScan(true /* is_performing_initial_scan */);
    }
    return;
  }

  if (is_performing_initial_scan_) {
    if (is_bluetooth_connection_to_phone_active_ &&
        !has_received_first_remote_status_) {
      RecordGetRemoteStatusResultFailure(
          GetRemoteStatusResultFailureReason::kCanceledBluetoothDisabled);
    } else {
      RecordFindAndConnectToHostResult(
          FindAndConnectToHostResult::kCanceledBluetoothDisabled);
    }

    SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    return;
  }

  // If Bluetooth is off but no initial scan is active, still ensure that the
  // lock screen UI reflects that Bluetooth is off.
  UpdateLockScreen();
}

bool UnlockManagerImpl::IsBluetoothAdapterRecoveringFromSuspend() const {
  return bluetooth_suspension_recovery_timer_->IsRunning();
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
    PA_LOG(VERBOSE) << "Already attempting auth.";
    return;
  }

  if (auth_type != mojom::AuthType::USER_CLICK)
    return;

  is_attempting_auth_ = true;

  if (!life_cycle_ || !GetMessenger()) {
    PA_LOG(ERROR) << "No life_cycle active when auth was attempted";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kNoPendingOrActiveHost);
    UpdateLockScreen();
    return;
  }

  if (!IsUnlockAllowed()) {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockNotAllowed);
    UpdateLockScreen();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &UnlockManagerImpl::FinalizeAuthAttempt,
          reject_auth_attempt_weak_ptr_factory_.GetWeakPtr(),
          SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
              kAuthAttemptTimedOut),
      kAuthAttemptTimeout);

  GetMessenger()->RequestUnlock();
}

void UnlockManagerImpl::CancelConnectionAttempt() {
  PA_LOG(VERBOSE) << "User entered password.";

  bluetooth_suspension_recovery_timer_->Stop();

  // Note: There is no need to record metrics here if Bluetooth isn't present
  // and powered; that has already been handled at this point in
  // OnBluetoothAdapterPresentAndPoweredChanged().
  if (!IsBluetoothPresentAndPowered())
    return;

  if (is_performing_initial_scan_) {
    if (is_bluetooth_connection_to_phone_active_ &&
        !has_received_first_remote_status_) {
      RecordGetRemoteStatusResultFailure(
          GetRemoteStatusResultFailureReason::kCanceledUserEnteredPassword);
    } else {
      RecordFindAndConnectToHostResult(
          FindAndConnectToHostResult::kCanceledUserEnteredPassword);
    }

    SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
  }
}

std::unique_ptr<ProximityMonitor> UnlockManagerImpl::CreateProximityMonitor(
    RemoteDeviceLifeCycle* life_cycle) {
  return std::make_unique<ProximityMonitorImpl>(life_cycle->GetRemoteDevice(),
                                                life_cycle->GetChannel());
}

SmartLockState UnlockManagerImpl::GetSmartLockState() {
  if (!life_cycle_)
    return SmartLockState::kInactive;

  if (!IsBluetoothPresentAndPowered())
    return SmartLockState::kBluetoothDisabled;

  if (IsUnlockAllowed())
    return SmartLockState::kPhoneAuthenticated;

  RemoteDeviceLifeCycle::State life_cycle_state = life_cycle_->GetState();
  if (life_cycle_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED)
    return SmartLockState::kPhoneNotAuthenticated;

  if (is_performing_initial_scan_)
    return SmartLockState::kConnectingToPhone;

  Messenger* messenger = GetMessenger();

  // Show a timeout state if we can not connect to the remote device in a
  // reasonable amount of time.
  if (!messenger)
    return SmartLockState::kPhoneNotFound;

  // If the RSSI is too low, then the remote device is nowhere near the local
  // device. This message should take priority over messages about Smart Lock
  // states.
  if (proximity_monitor_ && !proximity_monitor_->IsUnlockAllowed()) {
    if (remote_screenlock_state_ &&
        *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED) {
      return SmartLockState::kPhoneFoundUnlockedAndDistant;
    } else {
      return SmartLockState::kPhoneFoundLockedAndDistant;
    }
  }

  if (remote_screenlock_state_) {
    switch (*remote_screenlock_state_) {
      case RemoteScreenlockState::DISABLED:
        return SmartLockState::kPhoneNotLockable;

      case RemoteScreenlockState::LOCKED:
        return SmartLockState::kPhoneFoundLockedAndProximate;

      case RemoteScreenlockState::PRIMARY_USER_ABSENT:
        return SmartLockState::kPrimaryUserAbsent;

      case RemoteScreenlockState::UNKNOWN:
      case RemoteScreenlockState::UNLOCKED:
        // Handled by the code below.
        break;
    }
  }

  if (messenger) {
    PA_LOG(WARNING) << "Connection to host established, but remote screenlock "
                    << "state was either malformed or not received.";
  }

  // TODO(crbug.com/1233587): Add more granular error states
  return SmartLockState::kPhoneNotFound;
}

void UnlockManagerImpl::UpdateLockScreen() {
  AttemptToStartRemoteDeviceLifecycle();

  SmartLockState new_state = GetSmartLockState();
  if (smartlock_state_ == new_state)
    return;

  PA_LOG(INFO) << "Updating Smart Lock state from " << smartlock_state_
               << " to " << new_state;

  RecordFirstStatusShownToUser(new_state);

  proximity_auth_client_->UpdateSmartLockState(new_state);
  smartlock_state_ = new_state;
}

void UnlockManagerImpl::SetIsPerformingInitialScan(
    bool is_performing_initial_scan) {
  PA_LOG(VERBOSE) << "Initial scan state is ["
                  << (is_performing_initial_scan_ ? "active" : "inactive")
                  << "]. Requesting state ["
                  << (is_performing_initial_scan ? "active" : "inactive")
                  << "].";

  is_performing_initial_scan_ = is_performing_initial_scan;

  // Clear the waking up state after a timeout.
  initial_scan_timeout_weak_ptr_factory_.InvalidateWeakPtrs();
  if (is_performing_initial_scan_) {
    initial_scan_start_time_ = base::DefaultClock::GetInstance()->Now();
    has_received_first_remote_status_ = false;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UnlockManagerImpl::OnInitialScanTimeout,
                       initial_scan_timeout_weak_ptr_factory_.GetWeakPtr()),
        kWakingUpDuration);
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::OnInitialScanTimeout() {
  // Note: There is no need to record metrics here if Bluetooth isn't present
  // and powered; that has already been handled at this point in
  // OnBluetoothAdapterPresentAndPoweredChanged().
  if (!IsBluetoothPresentAndPowered())
    return;

  if (is_bluetooth_connection_to_phone_active_) {
    PA_LOG(ERROR) << "Successfully connected to host, but it did not provide "
                     "remote status update.";
    RecordGetRemoteStatusResultFailure(
        GetRemoteStatusResultFailureReason::
            kTimedOutDidNotReceiveRemoteStatusUpdate);
  } else {
    PA_LOG(INFO) << "Initial scan for host returned no result.";
    RecordFindAndConnectToHostResult(FindAndConnectToHostResult::kTimedOut);
  }

  SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
}

void UnlockManagerImpl::FinalizeAuthAttempt(
    const std::optional<
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason>& error) {
  if (error) {
    RecordAuthResultFailure(*error);
  }

  if (!is_attempting_auth_)
    return;

  // Cancel the pending task to time out the auth attempt.
  reject_auth_attempt_weak_ptr_factory_.InvalidateWeakPtrs();

  bool should_accept = !error;
  if (should_accept && proximity_monitor_)
    proximity_monitor_->RecordProximityMetricsOnAuthSuccess();

  is_attempting_auth_ = false;

  PA_LOG(VERBOSE) << "Finalizing unlock...";
  proximity_auth_client_->FinalizeUnlock(should_accept);
}

UnlockManagerImpl::RemoteScreenlockState
UnlockManagerImpl::GetScreenlockStateFromRemoteUpdate(
    RemoteStatusUpdate update) {
  switch (update.secure_screen_lock_state) {
    case SECURE_SCREEN_LOCK_DISABLED:
      return RemoteScreenlockState::DISABLED;

    case SECURE_SCREEN_LOCK_ENABLED:
      if (update.user_presence == USER_PRESENCE_SECONDARY ||
          update.user_presence == USER_PRESENCE_BACKGROUND) {
        return RemoteScreenlockState::PRIMARY_USER_ABSENT;
      }

      if (update.user_presence == USER_PRESENT)
        return RemoteScreenlockState::UNLOCKED;

      return RemoteScreenlockState::LOCKED;

    case SECURE_SCREEN_LOCK_STATE_UNKNOWN:
      return RemoteScreenlockState::UNKNOWN;
  }

  NOTREACHED_IN_MIGRATION();
  return RemoteScreenlockState::UNKNOWN;
}

Messenger* UnlockManagerImpl::GetMessenger() {
  // TODO(tengs): We should use a weak pointer to hold the Messenger instance
  // instead.
  if (!life_cycle_)
    return nullptr;
  return life_cycle_->GetMessenger();
}

void UnlockManagerImpl::RecordFirstRemoteStatusReceived(bool unlockable) {
  if (has_received_first_remote_status_)
    return;
  has_received_first_remote_status_ = true;

  RecordGetRemoteStatusResultSuccess();

  if (initial_scan_start_time_.is_null() ||
      attempt_get_remote_status_start_time_.is_null()) {
    PA_LOG(WARNING) << "Attempted to RecordFirstRemoteStatusReceived() "
                       "without initial timestamps recorded.";
    NOTREACHED_IN_MIGRATION();
    return;
  }

  const std::string histogram_status_suffix =
      GetHistogramStatusSuffix(unlockable);

  base::Time now = base::DefaultClock::GetInstance()->Now();
  base::TimeDelta start_scan_to_receive_first_remote_status_duration =
      now - initial_scan_start_time_;
  base::TimeDelta authentication_to_receive_first_remote_status_duration =
      now - attempt_get_remote_status_start_time_;

  RecordExtendedDurationTimerMetric(
      "SmartLock.Performance.StartScanToReceiveFirstRemoteStatusDuration."
      "Unlock",
      start_scan_to_receive_first_remote_status_duration);
  RecordExtendedDurationTimerMetric(
      "SmartLock.Performance.StartScanToReceiveFirstRemoteStatusDuration."
      "Unlock." +
          histogram_status_suffix,
      start_scan_to_receive_first_remote_status_duration);

  // This should be much less than 10 seconds, so use UmaHistogramTimes.
  base::UmaHistogramTimes(
      "SmartLock.Performance."
      "AuthenticationToReceiveFirstRemoteStatusDuration.Unlock",
      authentication_to_receive_first_remote_status_duration);
  base::UmaHistogramTimes(
      "SmartLock.Performance."
      "AuthenticationToReceiveFirstRemoteStatusDuration.Unlock." +
          histogram_status_suffix,
      authentication_to_receive_first_remote_status_duration);
}

void UnlockManagerImpl::RecordFirstStatusShownToUser(SmartLockState new_state) {
  std::optional<FirstSmartLockStatus> first_status =
      GetFirstSmartLockStatus(new_state);
  if (!first_status.has_value()) {
    return;
  }

  if (has_user_been_shown_first_status_) {
    return;
  }
  has_user_been_shown_first_status_ = true;

  if (show_lock_screen_time_.is_null()) {
    PA_LOG(WARNING) << "Attempted to RecordFirstStatusShownToUser() "
                       "without initial timestamp recorded.";
    NOTREACHED_IN_MIGRATION();
    return;
  }

  base::UmaHistogramEnumeration("SmartLock.FirstStatusToUser",
                                first_status.value());

  base::Time now = base::DefaultClock::GetInstance()->Now();
  base::TimeDelta show_lock_screen_to_show_first_status_to_user_duration =
      now - show_lock_screen_time_;

    RecordExtendedDurationTimerMetric(
        "SmartLock.Performance.ShowLockScreenToShowFirstStatusToUserDuration."
        "Unlock",
        show_lock_screen_to_show_first_status_to_user_duration);

    if (new_state == SmartLockState::kPhoneAuthenticated) {
      RecordExtendedDurationTimerMetric(
          "SmartLock.Performance.ShowLockScreenToShowFirstStatusToUserDuration."
          "Unlock.Unlockable",
          show_lock_screen_to_show_first_status_to_user_duration);
    } else if (HasCommunicatedWithPhone(new_state)) {
      // Only log to Unlock.Other if we aren't in an unlockable state since
      // that's covered by the other metric, and only if we are in a state that
      // indicates we were able to communicate with the phone over Bluetooth
      // since in all other cases the time to show the first status is highly
      // deterministic.
      RecordExtendedDurationTimerMetric(
          "SmartLock.Performance.ShowLockScreenToShowFirstStatusToUserDuration."
          "Unlock.Other",
          show_lock_screen_to_show_first_status_to_user_duration);
    }
}

void UnlockManagerImpl::ResetPerformanceMetricsTimestamps() {
  show_lock_screen_time_ = base::Time();
  initial_scan_start_time_ = base::Time();
  attempt_get_remote_status_start_time_ = base::Time();
}

void UnlockManagerImpl::SetBluetoothSuspensionRecoveryTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  bluetooth_suspension_recovery_timer_ = std::move(timer);
}

void UnlockManagerImpl::RecordGetRemoteStatusResultSuccess(bool success) {
  base::UmaHistogramBoolean("SmartLock.GetRemoteStatus.Unlock", success);
  get_remote_status_unlock_success_ = success;
}

void UnlockManagerImpl::RecordGetRemoteStatusResultFailure(
    GetRemoteStatusResultFailureReason failure_reason) {
  RecordGetRemoteStatusResultSuccess(false /* success */);
  base::UmaHistogramEnumeration("SmartLock.GetRemoteStatus.Unlock.Failure",
                                failure_reason);
  get_remote_status_unlock_failure_reason_ = failure_reason;
}

std::string UnlockManagerImpl::GetRemoteStatusResultFailureReasonToString(
    GetRemoteStatusResultFailureReason reason) {
  switch (reason) {
    case GetRemoteStatusResultFailureReason::kCanceledBluetoothDisabled:
      return "CanceledBluetoothDisabled";
    case GetRemoteStatusResultFailureReason::
        kDeprecatedTimedOutCouldNotEstablishAuthenticatedChannel:
      return "DeprecatedTimedOutCouldNotEstablishAuthenticatedChannel";
    case GetRemoteStatusResultFailureReason::
        kTimedOutDidNotReceiveRemoteStatusUpdate:
      return "TimedOutDidNotReceiveRemoteStatusUpdate";
    case GetRemoteStatusResultFailureReason::
        kDeprecatedUserEnteredPasswordWhileBluetoothDisabled:
      return "DeprecatedUserEnteredPasswordWhileBluetoothDisabled";
    case GetRemoteStatusResultFailureReason::kCanceledUserEnteredPassword:
      return "CanceledUserEnteredPassword";
    case GetRemoteStatusResultFailureReason::kAuthenticatedChannelDropped:
      return "AuthenticatedChannelDropped";
  }
}

std::string UnlockManagerImpl::GetLastRemoteStatusUnlockForLogging() {
  if (!get_remote_status_unlock_success_.has_value()) {
    return kGetRemoteStatusNone;
  }

  if (*get_remote_status_unlock_success_) {
    return kGetRemoteStatusSuccess;
  }

  if (!get_remote_status_unlock_failure_reason_.has_value()) {
    return kGetRemoteStatusNone;
  }

  return GetRemoteStatusResultFailureReasonToString(
      *get_remote_status_unlock_failure_reason_);
}

}  // namespace proximity_auth
