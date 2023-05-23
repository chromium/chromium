// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_CLIENT_H_

#include "ash/public/cpp/smartlock_state.h"
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
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_CLIENT_H_
