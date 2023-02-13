// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_RECOVERY_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_RECOVERY_UTILS_H_

#include "base/component_export.h"

class PrefService;

namespace base {
class FeatureList;
}  // namespace base

namespace ash {

// Sets up recovery feature value. This ensures that recovery can be featured in
// OOBE (depending on the `channel`) but can later be disabled by Finch when
// appropriate.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
void CreateFallbackFieldTrialForRecovery(bool is_stable_channel,
                                         base::FeatureList* feature_list);

// Returns `true` if the recovery opt-in UIs should be shown for the user, and
// `false` otherwise. `is_managed` states whether the user is managed.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
bool IsRecoveryOptInAvailable(bool is_managed);

// Returns `true` if the recovery auth factor should be activated (by default or
// by policy), and `false` otherwise.
// - For non-managed users this value should be
// used only in opt-in UIs. In-session - call cryptohome to find out whether
// recovery factor is configured.
// - For managed users this value may change due to
// the policy change and may not correspond to the actual state in cryptohome.
// `is_managed` states whether the user is managed.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
bool GetRecoveryDefaultState(bool is_managed, PrefService* prefs);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_RECOVERY_UTILS_H_
