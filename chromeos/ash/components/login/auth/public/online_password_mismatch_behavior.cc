// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/online_password_mismatch_behavior.h"

#include "ash/constants/ash_login_pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

bool ShouldAutoWipeOnOnlinePasswordMismatch(PrefService& local_state,
                                            const AccountId& account_id) {
  // Consumer accounts should never be automatically wiped. We verify the user's
  // enterprise-managed status directly from the pre-populated KnownUser list in
  // Local State to ensure safety before applying any destructive policies.
  // Note: For ephemeral managed users, this value evaluates to false. However,
  // since ephemeral users are automatically wiped upon logout/reboot anyway,
  // skipping the explicit AutoWipe flow here is completely safe and correct.
  user_manager::KnownUser known_user(&local_state);
  if (!known_user.GetIsEnterpriseManaged(account_id)) {
    return false;
  }

  // Only proceed with the wipe if the administrator has explicitly configured
  // the device to automatically wipe the cryptohome on an online password
  // mismatch.
  return local_state.GetInteger(prefs::kDeviceOnlinePasswordMismatchBehavior) ==
         static_cast<int>(DeviceOnlinePasswordMismatchBehavior::kAutoWipe);
}

}  // namespace ash
