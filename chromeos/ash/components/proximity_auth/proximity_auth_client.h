// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/smartlock_state.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_manager.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"

namespace proximity_auth {

// An interface that needs to be supplied to the Proximity Auth component by its
// embedder. There should be one |ProximityAuthClient| per
// |content::BrowserContext|.
class ProximityAuthClient {
 public:
  virtual ~ProximityAuthClient() {}

  // Updates the user pod on the lock screen to reflect the provided
  // Smart Lock state.
  virtual void UpdateSmartLockState(ash::SmartLockState state) = 0;

  // Finalizes an unlock attempt initiated by the user. If |success| is true,
  // the screen is unlocked; otherwise, the auth attempt is rejected. An auth
  // attempt must be in progress before calling this function.
  virtual void FinalizeUnlock(bool success) = 0;

  // Gets the wrapped challenge for the given |user_id| and |remote_public_key|
  // of the user's remote device. The challenge binds to the secure channel
  // using |channel_binding_data|.
  // |callback| will be invoked when the challenge is acquired.
  // TODO(b/227674947): Delete this method now that sign in with Smart Lock is
  // deprecated.
  virtual void GetChallengeForUserAndDevice(
      const std::string& user_email,
      const std::string& remote_public_key,
      const std::string& channel_binding_data,
      base::OnceCallback<void(const std::string& challenge)> callback) = 0;

  // Returns the manager responsible for EasyUnlock preferences.
  virtual ProximityAuthPrefManager* GetPrefManager() = 0;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_CLIENT_H_
