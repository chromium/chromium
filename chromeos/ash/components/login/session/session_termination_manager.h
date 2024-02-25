// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_SESSION_TERMINATION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_SESSION_TERMINATION_MANAGER_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"

namespace ash {

// SessionTerminationManager is used to handle the session termination.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION)
    SessionTerminationManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Occurs when the session will be terminated.
    virtual void OnSessionWillBeTerminated() {}
  };

  SessionTerminationManager();

  SessionTerminationManager(const SessionTerminationManager&) = delete;
  SessionTerminationManager& operator=(const SessionTerminationManager&) =
      delete;

  ~SessionTerminationManager();

  static SessionTerminationManager* Get();

  // To be called instead of SessionManagerClient::StopSession.
  void StopSession(login_manager::SessionStopReason reason);

  // To be called on login screen if the policy is set.
  void RebootIfNecessary();

  // To be called when the device gets locked to single user.
  void SetDeviceLockedToSingleUser();

  // To be called when the device has to be rebooted on the session end.
  void SetDeviceRebootOnSignoutForRemoteCommand(
      base::OnceClosure before_reboot_callback);

  // Returns whether the device is locked to single user.
  bool IsLockedToSingleUser();

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

 private:
  void DidWaitForServiceToBeAvailable(bool service_is_available);
  void ProcessCryptohomeLoginStatusReply(
      const std::optional<user_data_auth::GetLoginStatusReply>& reply);
  void RebootIfNecessaryProcessReply(
      std::optional<user_data_auth::GetLoginStatusReply> reply);

  base::ObserverList<Observer> observers_;
  bool is_locked_to_single_user_ = false;
  bool should_reboot_on_signout_ = false;
  base::OnceClosure before_reboot_callback_;
  base::WeakPtrFactory<SessionTerminationManager> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_SESSION_TERMINATION_MANAGER_H_
