// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/split_stores_and_local_upm.h"

#include "base/android/build_info.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "build/buildflag.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager {

namespace {

// DO NOT expose the enum nor the pref name in a header! This is a legacy pref
// and usages should be limited to GetLegacySplitStoresPref().
//
// Do not renumber UseUpmLocalAndSeparateStoresState, values are persisted.
// Values are also used for metrics recording.
enum class UseUpmLocalAndSeparateStoresState {
  kOff = 0,
  kOffAndMigrationPending = 1,
  kOn = 2,
  kMaxValue = kOn
};
constexpr char kPasswordsUseUPMLocalAndSeparateStores[] =
    "passwords_use_upm_local_and_separate_stores";

// Do not expose these constants! Use GetLocalUpmMinGmsVersion() instead.
const int kLocalUpmMinGmsVersionForNonAuto = 240212000;
const int kLocalUpmMinGmsVersionForAuto = 241512000;

}  // namespace

void RegisterLegacySplitStoresPref(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(UseUpmLocalAndSeparateStoresState::kOff));
}

bool GetLegacySplitStoresPref(const PrefService* pref_service) {
  switch (static_cast<UseUpmLocalAndSeparateStoresState>(
      pref_service->GetInteger(kPasswordsUseUPMLocalAndSeparateStores))) {
    case UseUpmLocalAndSeparateStoresState::kOff:
    case UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
      return false;
    case UseUpmLocalAndSeparateStoresState::kOn:
      return true;
  }
  NOTREACHED();
}

bool IsGmsCoreUpdateRequired(const syncer::SyncService* sync_service) {
#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  return false;
#else
  std::string gms_version_str =
      base::android::BuildInfo::GetInstance()->gms_version_code();
  int gms_version = 0;
  // GMSCore version could not be parsed, probably no GMSCore installed.
  if (!base::StringToInt(gms_version_str, &gms_version)) {
    return true;
  }

  // GMSCore version is pre-UPM, update is required.
  if (gms_version < kAccountUpmMinGmsVersion) {
    return true;
  }

  // GMSCore version is post-UPM with local passwords, no update required.
  if (gms_version >= GetLocalUpmMinGmsVersion()) {
    return false;
  }

  // GMSCore supports account storage only, thus update is required if password
  // syncing is disabled.
  if (!sync_util::HasChosenToSyncPasswords(sync_service)) {
    return true;
  }

  return false;
#endif  //  BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
}

int GetLocalUpmMinGmsVersion() {
  return base::android::BuildInfo::GetInstance()->is_automotive()
             ? kLocalUpmMinGmsVersionForAuto
             : kLocalUpmMinGmsVersionForNonAuto;
}

void SetLegacySplitStoresPrefForTest(PrefService* pref_service, bool enabled) {
  pref_service->SetInteger(
      kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(enabled ? UseUpmLocalAndSeparateStoresState::kOn
                               : UseUpmLocalAndSeparateStoresState::kOff));
}

}  // namespace password_manager
