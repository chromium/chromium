// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace password_bubble_experiment {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      password_manager::prefs::kWasSignInPasswordPromoClicked, false);

  registry->RegisterIntegerPref(
      password_manager::prefs::kNumberSignInPasswordPromoShown, 0);

  registry->RegisterBooleanPref(
      password_manager::prefs::kSignInPasswordPromoRevive, false);
}

int GetSmartBubbleDismissalThreshold() {
  return 3;
}

bool IsSmartLockUser(const syncer::SyncService* sync_service) {
  return password_manager_util::GetPasswordSyncState(sync_service) !=
         password_manager::SyncState::kNotSyncing;
}

bool ShouldShowAutoSignInPromptFirstRunExperience(PrefService* prefs) {
  return !prefs->GetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown);
}

void RecordAutoSignInPromptFirstRunExperienceWasShown(PrefService* prefs) {
  prefs->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, true);
}

void TurnOffAutoSignin(PrefService* prefs) {
  prefs->SetBoolean(password_manager::prefs::kCredentialsEnableAutosignin,
                    false);
}

bool ShouldShowChromeSignInPasswordPromo(
    PrefService* prefs,
    const syncer::SyncService* sync_service) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  // If the account-scoped storage for passwords is enabled, then the user
  // doesn't need to enable the full Sync feature to get their account
  // passwords, so suppress the promo in this case.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return false;
  }

  if (!prefs->GetBoolean(prefs::kSigninAllowed))
    return false;

  if (!sync_service ||
      sync_service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE) ||
      sync_service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      sync_service->GetUserSettings()->IsFirstSetupComplete()) {
    return false;
  }
  if (!prefs->GetBoolean(password_manager::prefs::kSignInPasswordPromoRevive)) {
    // Reset the counters so that the promo is shown again.
    prefs->SetBoolean(password_manager::prefs::kSignInPasswordPromoRevive,
                      true);
    prefs->ClearPref(password_manager::prefs::kWasSignInPasswordPromoClicked);
    prefs->ClearPref(password_manager::prefs::kNumberSignInPasswordPromoShown);
  }
  // Don't show the promo more than 3 times.
  constexpr int kThreshold = 3;
  return !prefs->GetBoolean(
             password_manager::prefs::kWasSignInPasswordPromoClicked) &&
         prefs->GetInteger(
             password_manager::prefs::kNumberSignInPasswordPromoShown) <
             kThreshold;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace password_bubble_experiment
