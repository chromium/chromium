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
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"


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
  // First check for `kHadBiometricsAvailable` ensures that user have biometric
  // scanner on their devices, second check
  // `kBiometricAuthenticationBeforeFilling` ensures that toggle in settings
  // that manages this feature is turned on.
  return local_state_ &&
         local_state_->GetBoolean(
             password_manager::prefs::kHadBiometricsAvailable) &&
         pref_service_ &&
         pref_service_->GetBoolean(
             password_manager::prefs::kBiometricAuthenticationBeforeFilling);
#else
  return false;
#endif
}

bool PasswordFeatureManagerImpl::IsAccountStorageEnabled() const {
  return features_util::IsAccountStorageEnabled(sync_service_);
}

features_util::PasswordAccountStorageUsageLevel
PasswordFeatureManagerImpl::ComputePasswordAccountStorageUsageLevel() const {
  return features_util::ComputePasswordAccountStorageUsageLevel(sync_service_);
}

}  // namespace password_manager
