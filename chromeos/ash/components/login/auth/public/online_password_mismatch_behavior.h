// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_ONLINE_PASSWORD_MISMATCH_BEHAVIOR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_ONLINE_PASSWORD_MISMATCH_BEHAVIOR_H_

#include "base/component_export.h"

class AccountId;
class PrefService;

namespace ash {

// Maps the values of the same-named policy located in
// components/policy/resources/templates/policy_definitions/Signin/DeviceOnlinePasswordMismatchBehavior.yaml.
enum class DeviceOnlinePasswordMismatchBehavior {
  // The user is prompted to enter their old password or use cryptohome
  // recovery.
  kDefault = 0,
  // The user's cryptohome is automatically wiped and recreated.
  kAutoWipe = 1,
  kMaxValue = kAutoWipe,
};

// Returns true if the device is configured to automatically wipe the
// cryptohome on online password mismatch (DeviceOnlinePasswordMismatchBehavior
// policy), and `account_id` is an enterprise-managed account.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
bool ShouldAutoWipeOnOnlinePasswordMismatch(PrefService& local_state,
                                            const AccountId& account_id);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_ONLINE_PASSWORD_MISMATCH_BEHAVIOR_H_
