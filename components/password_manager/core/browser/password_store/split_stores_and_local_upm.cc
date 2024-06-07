// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"

#include "base/android/build_info.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

using password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore;
using password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;

namespace password_manager {

bool UsesSplitStoresAndUPMForLocal(const PrefService* pref_service) {
  switch (
      static_cast<UseUpmLocalAndSeparateStoresState>(pref_service->GetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores))) {
    case UseUpmLocalAndSeparateStoresState::kOff:
    case UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
      return false;
    case UseUpmLocalAndSeparateStoresState::kOn:
      return true;
  }
  NOTREACHED_NORETURN();
}

bool IsGmsCoreUpdateRequired(const PrefService* pref_service,
                             const syncer::SyncService* sync_service,
                             const std::string& gms_version_str) {
  if (!features::IsUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled()) {
    return false;
  }

  int gms_version = 0;
  // GMSCore version could not be parsed, probably no GMSCore installed.
  if (!base::StringToInt(gms_version_str, &gms_version)) {
    return true;
  }

  // GMSCore version is pre-UPM, update is required.
  if (gms_version < password_manager::features::kAccountUpmMinGmsVersion) {
    return true;
  }

  // GMSCore version is post-UPM with local passwords, no update required.
  bool is_automotive = base::android::BuildInfo::GetInstance()->is_automotive();
  if (is_automotive &&
      gms_version >= base::GetFieldTrialParamByFeatureAsInt(
                         kUnifiedPasswordManagerSyncOnlyInGMSCore,
                         features::kLocalUpmMinGmsVersionParamForAuto,
                         features::kDefaultLocalUpmMinGmsVersionForAuto)) {
    return false;
  }
  if (!is_automotive &&
      gms_version >= base::GetFieldTrialParamByFeatureAsInt(
                         kUnifiedPasswordManagerSyncOnlyInGMSCore,
                         features::kLocalUpmMinGmsVersionParam,
                         features::kDefaultLocalUpmMinGmsVersion)) {
    return false;
  }

  // GMSCore supports account storage only, thus update is required if password
  // syncing is disabled.
  // TODO(crbug.com/344609691): Re-arrange build targets so
  // password_manager::sync_util::HasChosenToSyncPasswords() can be called here
  // without causing a cyclic dependency.
  bool has_chosen_to_sync_passwords =
      sync_service && sync_service->GetDisableReasons().empty() &&
      sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPasswords);
  if (!has_chosen_to_sync_passwords) {
    return true;
  }

  // There are no local passwords so GMSCore can be used regardless of
  // unenrolled or initial UPM migration status.
  if (pref_service->GetBoolean(prefs::kEmptyProfileStoreLoginDatabase)) {
    return false;
  }

  // If the user was unenrolled or has never done the initial migration, update
  // to the GMSCore version with local passwords support is required.
  bool is_initial_migration_missing =
      pref_service->GetInteger(
          kCurrentMigrationVersionToGoogleMobileServices) == 0;
  bool is_user_unenrolled = pref_service->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
  return is_user_unenrolled || is_initial_migration_missing;
}

}  // namespace password_manager
