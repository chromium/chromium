// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_SESSION_SESSION_TERMINATION_MANAGER_H_
#define CHROMEOS_LOGIN_SESSION_SESSION_TERMINATION_MANAGER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"

namespace chromeos {

// SessionTerminationManager is used to handle the session termination.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_SESSION) SessionTerminationManager {
 public:
  SessionTerminationManager();
  ~SessionTerminationManager();

  static SessionTerminationManager* Get();

  // To be called instead of SessionManagerClient::StopSession.
  void StopSession();

  // To be called on login screen if the policy is set.
  void RebootIfNecessary();

  // To be called when the device gets locked to single user.
  void SetDeviceLockedToSingleUser();

  // Returns whether the device is locked to single user.
  bool IsLockedToSingleUser();

 private:
  void DidWaitForServiceToBeAvailable(bool service_is_available);
  void ProcessTpmStatusReply(
      const base::Optional<cryptohome::BaseReply>& reply);
  void Reboot();
  void RebootIfNecessaryProcessReply(
      base::Optional<cryptohome::BaseReply> reply);

  bool is_locked_to_single_user_ = false;
  base::WeakPtrFactory<SessionTerminationManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionTerminationManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_SESSION_SESSION_TERMINATION_MANAGER_H_
