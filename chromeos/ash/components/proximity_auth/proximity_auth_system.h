// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace secure_channel {
class SecureChannelClient;
}
}  // namespace ash

namespace proximity_auth {

class ProximityAuthClient;
class RemoteDeviceLifeCycle;
class UnlockManager;

// This is the main entry point to start Proximity Auth, the underlying system
// for the Smart Lock feature. Given a list of remote devices (i.e. a
// phone) for each registered user, the system will handle the connection,
// authentication, and messenging protocol when the screen is locked and the
// registered user is focused.
class ProximityAuthSystem : public ScreenlockBridge::Observer {
 public:
  ProximityAuthSystem(
      ProximityAuthClient* proximity_auth_client,
      ash::secure_channel::SecureChannelClient* secure_channel_client);

  ProximityAuthSystem(const ProximityAuthSystem&) = delete;
  ProximityAuthSystem& operator=(const ProximityAuthSystem&) = delete;

  ~ProximityAuthSystem() override;

  // Starts the system to connect and authenticate when a registered user is
  // focused on the lock screen.
  void Start();

  // Stops the system.
  void Stop();

  // Registers a list of |remote_devices| for |account_id| that can be used for
  // unlock. |local_device| represents this device (i.e. this Chrome OS
  // device) for this particular user profile context. If devices were
  // previously registered for the user, then they will be replaced.
  void SetRemoteDevicesForUser(
      const AccountId& account_id,
      const ash::multidevice::RemoteDeviceRefList& remote_devices,
      std::optional<ash::multidevice::RemoteDeviceRef> local_device);

  // Returns the RemoteDevices registered for |account_id|. Returns an empty
  // list if no devices are registered for |account_id|.
  ash::multidevice::RemoteDeviceRefList GetRemoteDevicesForUser(
      const AccountId& account_id) const;

  // Called when the user clicks the user pod and attempts to unlock.
  void OnAuthAttempted();

  // Called when the system suspends.
  void OnSuspend();

  // Called when the system wakes up from a suspended state.
  void OnSuspendDone();

  // Called in order to disable attempts to get RemoteStatus from host devices.
  void CancelConnectionAttempt();

  // The last value emitted to the SmartLock.GetRemoteStatus.Unlock(.Failure)
  // metrics. Helps to understand whether/why not Smart Lock was an available
  // choice for unlock. Returns the empty string if |unlock_manager_| is
  // nullptr.
  std::string GetLastRemoteStatusUnlockForLogging();

 protected:
  // Constructor which allows passing in a custom |unlock_manager_|.
  // Exposed for testing.
  ProximityAuthSystem(
      ash::secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<UnlockManager> unlock_manager);

  // Creates the RemoteDeviceLifeCycle for |remote_device| and |local_device|.
  // |remote_device| is the host intended to be connected to, and |local_device|
  // represents this device (i.e. this Chrome OS device) for this particular
  // user profile context.
  // Exposed for testing.
  virtual std::unique_ptr<RemoteDeviceLifeCycle> CreateRemoteDeviceLifeCycle(
      ash::multidevice::RemoteDeviceRef remote_device,
      std::optional<ash::multidevice::RemoteDeviceRef> local_device);

  // ScreenlockBridge::Observer:
  void OnScreenDidLock() override;
  void OnScreenDidUnlock() override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

 private:
  // Lists of remote devices, keyed by user account id.
  std::map<AccountId, ash::multidevice::RemoteDeviceRefList>
      remote_devices_map_;

  // A mapping from each profile's account ID to the profile-specific
  // representation of this device (i.e. this Chrome OS device) for that
  // particular user profile.
  std::map<AccountId, ash::multidevice::RemoteDeviceRef> local_device_map_;

  // Entry point to the SecureChannel API.
  raw_ptr<ash::secure_channel::SecureChannelClient> secure_channel_client_;

  // Responsible for the life cycle of connecting and authenticating to
  // the RemoteDevice of the currently focused user.
  std::unique_ptr<RemoteDeviceLifeCycle> remote_device_life_cycle_;

  // Handles the interaction with the lock screen UI.
  std::unique_ptr<UnlockManager> unlock_manager_;

  // True if the system is suspended.
  bool suspended_ = false;

  // True if the system is started_.
  bool started_ = false;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H_
