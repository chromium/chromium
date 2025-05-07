// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/system_features_disable_list/system_features_disable_list_policy_utils.h"

#include "base/check.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/system_features_disable_list_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace policy {

void RegisterDisabledSystemFeaturesPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(policy_prefs::kSystemFeaturesDisableList);
  registry->RegisterStringPref(policy_prefs::kSystemFeaturesDisableMode,
                               kSystemFeaturesDisableModeBlocked);
}

bool IsDisabledAppsModeHidden(const PrefService& local_state) {
  const bool is_disabled_apps_mode_hidden_pref =
      local_state.GetString(policy::policy_prefs::kSystemFeaturesDisableMode) ==
      kSystemFeaturesDisableModeHidden;

  // If explicitly set by admin policy, honor that value. regardless of session
  // type. This applies regardless of session type (although not exposed for
  // user sessions, it can still be set e.g. via an on-device policy).
  if (local_state.IsManagedPreference(
          policy::policy_prefs::kSystemFeaturesDisableMode)) {
    return is_disabled_apps_mode_hidden_pref;
  }

  // Legacy behavior (flag is off): Use the raw SystemFeaturesDisableMode pref's
  // value for all session types. Typically this should default to "blocked" for
  // regular user sessions.
  if (!chromeos::features::IsSystemFeaturesDisableListHiddenEnabled()) {
    return is_disabled_apps_mode_hidden_pref;
  }

  CHECK(user_manager::UserManager::IsInitialized())
      << "IsDisabledAppsModeHidden() requires UserManager initialization.";

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();

  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user) {
    return false;
  }

  if (!active_user->is_managed()) {
    return false;
  }

  // For Managed Guest Sessions (MGS): Behavior remains determined by the
  // SystemFeaturesDisableMode pref's current value.
  if (active_user->IsDeviceLocalAccount()) {
    return is_disabled_apps_mode_hidden_pref;
  }

  // New default for non-MGS sessions: force "hidden" mode for regular managed
  // users.
  return true;
}

}  // namespace policy
