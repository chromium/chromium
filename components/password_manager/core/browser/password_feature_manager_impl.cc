// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_feature_manager_impl.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

PasswordFeatureManagerImpl::PasswordFeatureManagerImpl(
    PrefService* pref_service,
    PrefService* local_state,
    const syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      local_state_(local_state),
      sync_service_(sync_service) {}

bool PasswordFeatureManagerImpl::IsGenerationEnabled() const {
  switch (password_manager_util::GetPasswordSyncState(sync_service_)) {
    case SyncState::kNotSyncing:
      return ShouldShowAccountStorageOptIn();
    case SyncState::kSyncingWithCustomPassphrase:
    case SyncState::kSyncingNormalEncryption:
    case SyncState::kAccountPasswordsActiveNormalEncryption:
    case SyncState::kAccountPasswordsActiveWithCustomPassphrase:
      return true;
  }
}

bool PasswordFeatureManagerImpl::IsBiometricAuthenticationBeforeFillingEnabled()
    const {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
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
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricAuthenticationForFilling) &&
         pref_service_ &&
         pref_service_->GetBoolean(
             password_manager::prefs::kBiometricAuthenticationBeforeFilling);
#else
  return false;
#endif
}

bool PasswordFeatureManagerImpl::IsOptedInForAccountStorage() const {
  return features_util::IsOptedInForAccountStorage(pref_service_,
                                                   sync_service_);
}

bool PasswordFeatureManagerImpl::ShouldShowAccountStorageOptIn() const {
  return features_util::ShouldShowAccountStorageOptIn(pref_service_,
                                                      sync_service_);
}

bool PasswordFeatureManagerImpl::ShouldShowAccountStorageReSignin(
    const GURL& current_page_url) const {
  return features_util::ShouldShowAccountStorageReSignin(
      pref_service_, sync_service_, current_page_url);
}

bool PasswordFeatureManagerImpl::ShouldShowAccountStorageBubbleUi() const {
  return features_util::ShouldShowAccountStorageBubbleUi(pref_service_,
                                                         sync_service_);
}

PasswordForm::Store PasswordFeatureManagerImpl::GetDefaultPasswordStore()
    const {
  DCHECK(pref_service_);
  return features_util::GetDefaultPasswordStore(pref_service_, sync_service_);
}

bool PasswordFeatureManagerImpl::IsDefaultPasswordStoreSet() const {
  return features_util::IsDefaultPasswordStoreSet(pref_service_, sync_service_);
}

metrics_util::PasswordAccountStorageUsageLevel
PasswordFeatureManagerImpl::ComputePasswordAccountStorageUsageLevel() const {
  return features_util::ComputePasswordAccountStorageUsageLevel(pref_service_,
                                                                sync_service_);
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
void PasswordFeatureManagerImpl::OptInToAccountStorage() {
  features_util::OptInToAccountStorage(pref_service_, sync_service_);
}

void PasswordFeatureManagerImpl::OptOutOfAccountStorageAndClearSettings() {
  features_util::OptOutOfAccountStorageAndClearSettings(pref_service_,
                                                        sync_service_);
}

void PasswordFeatureManagerImpl::SetDefaultPasswordStore(
    const PasswordForm::Store& store) {
  features_util::SetDefaultPasswordStore(pref_service_, sync_service_, store);
}

bool PasswordFeatureManagerImpl::
    ShouldOfferOptInAndMoveToAccountStoreAfterSavingLocally() const {
  return ShouldShowAccountStorageOptIn() && !IsDefaultPasswordStoreSet();
}

void PasswordFeatureManagerImpl::RecordMoveOfferedToNonOptedInUser() {
  features_util::RecordMoveOfferedToNonOptedInUser(pref_service_,
                                                   sync_service_);
}

int PasswordFeatureManagerImpl::GetMoveOfferedToNonOptedInUserCount() const {
  return features_util::GetMoveOfferedToNonOptedInUserCount(pref_service_,
                                                            sync_service_);
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
