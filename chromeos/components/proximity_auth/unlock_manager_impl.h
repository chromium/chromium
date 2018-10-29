// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chromeos/components/proximity_auth/messenger_observer.h"
#include "chromeos/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/components/proximity_auth/proximity_monitor_observer.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/components/proximity_auth/remote_status_update.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/screenlock_state.h"
#include "chromeos/components/proximity_auth/unlock_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace proximity_auth {

class Messenger;
class ProximityAuthClient;
class ProximityAuthPrefManager;
class ProximityMonitor;

// The unlock manager is responsible for controlling the lock screen UI based on
// the authentication status of the registered remote devices.
class UnlockManagerImpl : public UnlockManager,
                          public MessengerObserver,
                          public ProximityMonitorObserver,
                          public ScreenlockBridge::Observer,
                          chromeos::PowerManagerClient::Observer,
                          public device::BluetoothAdapter::Observer {
 public:
  // The |proximity_auth_client| is not owned and should outlive the constructed
  // unlock manager.
  UnlockManagerImpl(ProximityAuthSystem::ScreenlockType screenlock_type,
                    ProximityAuthClient* proximity_auth_client,
                    ProximityAuthPrefManager* pref_manager);
  ~UnlockManagerImpl() override;

  // UnlockManager:
  bool IsUnlockAllowed() override;
  void SetRemoteDeviceLifeCycle(RemoteDeviceLifeCycle* life_cycle) override;
  void OnLifeCycleStateChanged() override;
  void OnAuthAttempted(mojom::AuthType auth_type) override;

 protected:
  // Creates a ProximityMonitor instance for the given |connection|.
  // Exposed for testing.
  virtual std::unique_ptr<ProximityMonitor> CreateProximityMonitor(
      RemoteDeviceLifeCycle* life_cycle,
      ProximityAuthPrefManager* pref_manager);

 private:
  // The possible lock screen states for the remote device.
  enum class RemoteScreenlockState {
    UNKNOWN,
    UNLOCKED,
    DISABLED,
    LOCKED,
  };

  // MessengerObserver:
  void OnUnlockEventSent(bool success) override;
  void OnRemoteStatusUpdate(const RemoteStatusUpdate& status_update) override;
  void OnDecryptResponse(const std::string& decrypted_bytes) override;
  void OnUnlockResponse(bool success) override;
  void OnDisconnected() override;

  // ProximityMonitorObserver:
  void OnProximityStateChanged() override;

  // ScreenlockBridge::Observer
  void OnScreenDidLock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

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
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // Returns true if the BluetoothAdapter is present and powered.
  bool IsBluetoothPresentAndPowered() const;

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

  // Sets waking up state.
  void SetWakingUpState(bool is_waking_up);

  // Accepts or rejects the current auth attempt according to |should_accept|.
  // If the auth attempt is accepted, unlocks the screen.
  void AcceptAuthAttempt(bool should_accept);

  // Returns the screen lock state corresponding to the given remote |status|
  // update.
  RemoteScreenlockState GetScreenlockStateFromRemoteUpdate(
      RemoteStatusUpdate update);

  // Returns the Messenger instance associated with |life_cycle_|. This function
  // will return nullptr if |life_cycle_| is not set or the remote device is not
  // yet authenticated.
  Messenger* GetMessenger();

  // Whether |this| manager is being used for sign-in or session unlock.
  const ProximityAuthSystem::ScreenlockType screenlock_type_;

  // Whether the user is present at the remote device. Unset if no remote status
  // update has yet been received.
  std::unique_ptr<RemoteScreenlockState> remote_screenlock_state_;

  // Controls the proximity auth flow logic for a remote device. Not owned, and
  // expcted to outlive |this| instance.
  RemoteDeviceLifeCycle* life_cycle_;

  // Tracks whether the remote device is currently in close enough proximity to
  // the local device to allow unlocking.
  std::unique_ptr<ProximityMonitor> proximity_monitor_;

  // Used to call into the embedder. Expected to outlive |this| instance.
  ProximityAuthClient* proximity_auth_client_;

  // Used to access the common prefs. Expected to outlive |this| instance.
  ProximityAuthPrefManager* pref_manager_;

  // Whether the screen is currently locked.
  bool is_locked_;

  // True if the manager is currently processing a user-initiated authentication
  // attempt, which is initiated when the user pod is clicked.
  bool is_attempting_auth_;

  // Whether the system is waking up from sleep.
  bool is_waking_up_;

  // The Bluetooth adapter. Null if there is no adapter present on the local
  // device.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // The sign-in secret received from the remote device by decrypting the
  // sign-in challenge.
  std::unique_ptr<std::string> sign_in_secret_;

  // The state of the current screen lock UI.
  ScreenlockState screenlock_state_;

  // Used to clear the waking up state after a timeout.
  base::WeakPtrFactory<UnlockManagerImpl>
      clear_waking_up_state_weak_ptr_factory_;

  // Used to reject auth attempts after a timeout. An in-progress auth attempt
  // blocks the sign-in screen UI, so it's important to prevent the auth attempt
  // from blocking the UI in case a step in the code path hangs.
  base::WeakPtrFactory<UnlockManagerImpl> reject_auth_attempt_weak_ptr_factory_;

  // Used to vend all other weak pointers.
  base::WeakPtrFactory<UnlockManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UnlockManagerImpl);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_IMPL_H_
