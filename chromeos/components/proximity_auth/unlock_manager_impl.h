// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromeos/components/proximity_auth/messenger_observer.h"
#include "chromeos/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/components/proximity_auth/proximity_monitor_observer.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/components/proximity_auth/remote_status_update.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/screenlock_state.h"
#include "chromeos/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/components/proximity_auth/unlock_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace proximity_auth {

class Messenger;
class ProximityAuthClient;
class ProximityMonitor;

// The unlock manager is responsible for controlling the lock screen UI based on
// the authentication status of the registered remote devices.
class UnlockManagerImpl : public UnlockManager,
                          public MessengerObserver,
                          public ProximityMonitorObserver,
                          public chromeos::PowerManagerClient::Observer,
                          public device::BluetoothAdapter::Observer,
                          public RemoteDeviceLifeCycle::Observer {
 public:
  // The |proximity_auth_client| is not owned and should outlive the constructed
  // unlock manager.
  UnlockManagerImpl(ProximityAuthSystem::ScreenlockType screenlock_type,
                    ProximityAuthClient* proximity_auth_client);
  ~UnlockManagerImpl() override;

  // UnlockManager:
  bool IsUnlockAllowed() override;
  void SetRemoteDeviceLifeCycle(RemoteDeviceLifeCycle* life_cycle) override;
  void OnAuthAttempted(mojom::AuthType auth_type) override;
  void CancelConnectionAttempt() override;

 protected:
  // Creates a ProximityMonitor instance for the given |connection|.
  // Exposed for testing.
  virtual std::unique_ptr<ProximityMonitor> CreateProximityMonitor(
      RemoteDeviceLifeCycle* life_cycle);

 private:
  friend class ProximityAuthUnlockManagerImplTest;

  // The possible lock screen states for the remote device.
  enum class RemoteScreenlockState {
    UNKNOWN,
    UNLOCKED,
    DISABLED,
    LOCKED,
    PRIMARY_USER_ABSENT,
  };

  // MessengerObserver:
  void OnUnlockEventSent(bool success) override;
  void OnRemoteStatusUpdate(const RemoteStatusUpdate& status_update) override;
  void OnDecryptResponse(const std::string& decrypted_bytes) override;
  void OnUnlockResponse(bool success) override;
  void OnDisconnected() override;

  // ProximityMonitorObserver:
  void OnProximityStateChanged() override;

  // Called when the screenlock state changes.
  void OnScreenLockedOrUnlocked(bool is_locked);

  // Called when the Bluetooth adapter is initialized.
  void OnBluetoothAdapterInitialized(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // RemoteDeviceLifeCycle::Observer:
  void OnLifeCycleStateChanged(RemoteDeviceLifeCycle::State old_state,
                               RemoteDeviceLifeCycle::State new_state) override;

  // Returns true if the BluetoothAdapter is present and powered.
  bool IsBluetoothPresentAndPowered() const;

  // TODO(crbug.com/986896): Waiting a certain time, after resume, before
  // trusting the presence and power values returned by BluetoothAdapter is
  // necessary because the BluetoothAdapter returns incorrect values directly
  // after resume, and does not return correct values until about 1-2 seconds
  // later. Remove this function once the bug is resolved.
  //
  // This function returns true if the BluetoothAdapter is still resuming from
  // suspension, indicating that its returned presence and power values cannot
  // yet be trusted.
  bool IsBluetoothAdapterRecoveringFromSuspend() const;

  // Called once BluetoothAdapter has recovered after resuming from suspend,
  // meaning its presence and power values can be trusted again. This method
  // checks if Bluetooth is enabled; if it is not, it cancels the initial scan.
  void OnBluetoothAdapterPresentAndPoweredChanged();

  // If the RemoteDeviceLifeCycle is available, ensure it is started (but only
  // if Bluetooth is available).
  void AttemptToStartRemoteDeviceLifecycle();

  // Called when auth is attempted to send the sign-in challenge to the remote
  // device for decryption.
  void SendSignInChallenge();

  // Once the connection metadata is received from a ClientChannel, its channel
  // binding data can be used to finish a sign-in request.
  void OnGetConnectionMetadata(
      chromeos::secure_channel::mojom::ConnectionMetadataPtr
          connection_metadata_ptr);

  // Called with the sign-in |challenge| so we can send it to the remote device
  // for decryption.
  void OnGotSignInChallenge(const std::string& challenge);

  // Returns the current state for the screen lock UI.
  ScreenlockState GetScreenlockState();

  // Updates the lock screen based on the manager's current state.
  void UpdateLockScreen();

  // Activates or deactivates the proximity monitor, as appropriate given the
  // current state of |this| unlock manager.
  void UpdateProximityMonitorState();

  // Sets if the "initial scan" is in progress. This state factors into what is
  // shown to the user. See |is_performing_initial_scan_| for more.
  void SetIsPerformingInitialScan(bool is_performing_initial_scan);

  // Accepts or rejects the current auth attempt according to |error|. Accepts
  // if and only if |error| is empty. If the auth attempt is accepted, unlocks
  // the screen.
  void FinalizeAuthAttempt(
      const base::Optional<
          SmartLockMetricsRecorder::SmartLockAuthResultFailureReason>& error);

  // Failed to create a connection to the host during the "initial scan". See
  // |is_performing_initial_scan_| for more.
  void OnInitialScanTimeout();

  // Returns the screen lock state corresponding to the given remote |status|
  // update.
  RemoteScreenlockState GetScreenlockStateFromRemoteUpdate(
      RemoteStatusUpdate update);

  // Returns the Messenger instance associated with |life_cycle_|. This function
  // will return nullptr if |life_cycle_| is not set or the remote device is not
  // yet authenticated.
  Messenger* GetMessenger();

  // Records UMA performance metrics for the first remote status (regardless of
  // whether it's unlockable) being received.
  void RecordFirstRemoteStatusReceived(bool unlockable);

  // Records UMA performance metrics for the first status shown to the user
  // (regardless of whether it's unlockable/green).
  void RecordFirstStatusShownToUser(bool unlockable);

  // Clears the timers for beginning a scan and fetching remote status.
  void ResetPerformanceMetricsTimestamps();

  void SetBluetoothSuspensionRecoveryTimerForTesting(
      std::unique_ptr<base::OneShotTimer> timer);

  // Whether |this| manager is being used for sign-in or session unlock.
  const ProximityAuthSystem::ScreenlockType screenlock_type_;

  // Used to call into the embedder. Expected to outlive |this| instance.
  ProximityAuthClient* proximity_auth_client_;

  // Starts running after resuming from suspension, and fires once enough time
  // has elapsed such that the BluetoothAdapter's presence and power values can
  // be trusted again. To be removed once https://crbug.com/986896 is fixed.
  std::unique_ptr<base::OneShotTimer> bluetooth_suspension_recovery_timer_;

  // The Bluetooth adapter. Null if there is no adapter present on the local
  // device.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // Tracks whether the remote device is currently in close enough proximity to
  // the local device to allow unlocking.
  std::unique_ptr<ProximityMonitor> proximity_monitor_;

  // Whether the user is present at the remote device. Unset if no remote status
  // update has yet been received.
  std::unique_ptr<RemoteScreenlockState> remote_screenlock_state_;

  // The sign-in secret received from the remote device by decrypting the
  // sign-in challenge.
  std::unique_ptr<std::string> sign_in_secret_;

  // Controls the proximity auth flow logic for a remote device. Not owned, and
  // expcted to outlive |this| instance.
  RemoteDeviceLifeCycle* life_cycle_ = nullptr;

  // True if the manager is currently processing a user-initiated authentication
  // attempt, which is initiated when the user pod is clicked.
  bool is_attempting_auth_ = false;

  // If true, either the lock screen was just shown (after resuming from
  // suspend, or directly locking the screen), or the focused user pod was
  // switched. It becomes false if the phone is found, something goes wrong
  // while searching for the phone, or the initial scan times out (at which
  // point the user visually sees an indication that the phone cannot be found).
  // Though this field becomes false after this timeout, Smart Lock continues
  // to scan for the phone until the user unlocks the screen.
  bool is_performing_initial_scan_ = false;

  // True if a secure connection is currently active with the host.
  bool is_bluetooth_connection_to_phone_active_ = false;

  // TODO(crbug.com/986896): For a short time window after resuming from
  // suspension, BluetoothAdapter returns incorrect presence and power values.
  // This field acts as a cache in case we need to check those values during
  // that time window when the device resumes. Remove this field once the bug
  // is fixed.
  bool was_bluetooth_present_and_powered_before_last_suspend_ = false;

  // True only if the remote device has responded with a remote status, either
  // "unlockable" or otherwise.
  bool has_received_first_remote_status_ = false;

  // True only if the user has been shown a Smart Lock status and tooltip,
  // either "unlockable" (green) or otherwise (yellow).
  bool has_user_been_shown_first_status_ = false;

  // The state of the current screen lock UI.
  ScreenlockState screenlock_state_ = ScreenlockState::INACTIVE;

  // The timestamp of when the lock or login screen is shown to the user. Begins
  // when the screen is locked, the system is rebooted, the clamshell lid is
  // opened, or another user pod is switched to on the login screen.
  base::Time show_lock_screen_time_;

  // The timestamp of when UnlockManager begins to perform the initial scan for
  // the requested remote device of the provided RemoteDeviceLifeCycle. Usually
  // begins right after |show_lock_screen_time_|, unless Bluetooth is disabled.
  // If Bluetooth is re-enabled, it also begins.
  base::Time initial_scan_start_time_;

  // The timestamp of when UnlockManager successfully establishes a secure
  // connection to the requested remote device of the provided
  // RemoteDeviceLifeCycle, and begins to try to fetch its "remote status".
  base::Time attempt_get_remote_status_start_time_;

  // Used to track if the "initial scan" has timed out. See
  // |is_performing_initial_scan_| for more.
  base::WeakPtrFactory<UnlockManagerImpl>
      initial_scan_timeout_weak_ptr_factory_{this};

  // Used to reject auth attempts after a timeout. An in-progress auth attempt
  // blocks the sign-in screen UI, so it's important to prevent the auth attempt
  // from blocking the UI in case a step in the code path hangs.
  base::WeakPtrFactory<UnlockManagerImpl> reject_auth_attempt_weak_ptr_factory_{
      this};

  // Used to vend all other weak pointers.
  base::WeakPtrFactory<UnlockManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UnlockManagerImpl);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_IMPL_H_
