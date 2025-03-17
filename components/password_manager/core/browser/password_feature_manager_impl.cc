// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_feature_manager_impl.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace password_manager {

PasswordFeatureManagerImpl::PasswordFeatureManagerImpl(
    PrefService* pref_service,
    PrefService* local_state,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      local_state_(local_state),
      sync_service_(sync_service) {}

bool PasswordFeatureManagerImpl::IsGenerationEnabled() const {
  switch (password_manager::sync_util::GetPasswordSyncState(sync_service_)) {
    case sync_util::SyncState::kNotActive:
      return false;
    case sync_util::SyncState::kActiveWithNormalEncryption:
    case sync_util::SyncState::kActiveWithCustomPassphrase:
      return true;
  }
}

bool PasswordFeatureManagerImpl::IsBiometricAuthenticationBeforeFillingEnabled()
    const {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  // This checking order is important to ensure balanced experiment groups.
  // First check for `kHadBiometricsAvailable` ensures that user have biometric
  // scanner on their devices, shrinking down the amount of affected users.
  // Check for the feature flag happens for everyone no matter whether they
  // are/aren't using this feature, assuming they could use it(biometric scanner
  // is available). Final check `kBiometricAuthenticationBeforeFilling` ensures
  // that toggle in settings that manages this feature is turned on.
  return local_state_ &&
         local_state_->GetBoolean(
             password_manager::prefs::kHadBiometricsAvailable) &&
#if BUILDFLAG(IS_CHROMEOS)
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricsAuthForPwdFill) &&
#endif
         pref_service_ &&
         pref_service_->GetBoolean(
             password_manager::prefs::kBiometricAuthenticationBeforeFilling);
#else
  return false;
#endif
}

bool PasswordFeatureManagerImpl::IsAccountStorageEnabled() const {
  return features_util::IsAccountStorageEnabled(pref_service_, sync_service_);
}

features_util::PasswordAccountStorageUsageLevel
PasswordFeatureManagerImpl::ComputePasswordAccountStorageUsageLevel() const {
  return features_util::ComputePasswordAccountStorageUsageLevel(pref_service_,
                                                                sync_service_);
}

#if BUILDFLAG(IS_ANDROID)
bool PasswordFeatureManagerImpl::ShouldUpdateGmsCore() {
  return IsGmsCoreUpdateRequired(pref_service_, sync_service_);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
