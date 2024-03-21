// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_feature_manager.h"

namespace syncer {
class SyncService;
}  // namespace syncer

class PrefService;

namespace password_manager {

// Keeps track of which feature of PasswordManager is enabled for a given
// profile.
class PasswordFeatureManagerImpl : public PasswordFeatureManager {
 public:
  PasswordFeatureManagerImpl(PrefService* pref_service,
                             PrefService* local_state,
                             syncer::SyncService* sync_service);

  PasswordFeatureManagerImpl(const PasswordFeatureManagerImpl&) = delete;
  PasswordFeatureManagerImpl& operator=(const PasswordFeatureManagerImpl&) =
      delete;

  ~PasswordFeatureManagerImpl() override = default;

  bool IsGenerationEnabled() const override;

  bool IsBiometricAuthenticationBeforeFillingEnabled() const override;

  bool IsOptedInForAccountStorage() const override;
  bool ShouldShowAccountStorageOptIn() const override;
  bool ShouldShowAccountStorageReSignin(
      const GURL& current_page_url) const override;
  bool ShouldShowAccountStorageBubbleUi() const override;
  PasswordForm::Store GetDefaultPasswordStore() const override;
  bool IsDefaultPasswordStoreSet() const override;
  features_util::PasswordAccountStorageUsageLevel
  ComputePasswordAccountStorageUsageLevel() const override;

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  void OptInToAccountStorage() override;
  void OptOutOfAccountStorage() override;
  void OptOutOfAccountStorageAndClearSettings() override;
  bool ShouldOfferOptInAndMoveToAccountStoreAfterSavingLocally() const override;
  void SetDefaultPasswordStore(const PasswordForm::Store& store) override;
  bool ShouldChangeDefaultPasswordStore() const override;
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  bool ShouldUpdateGmsCore() override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<PrefService> local_state_;
  const raw_ptr<syncer::SyncService> sync_service_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_IMPL_H_
