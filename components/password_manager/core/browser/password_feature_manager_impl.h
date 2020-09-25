// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_IMPL_H_

#include "base/macros.h"
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
                             const syncer::SyncService* sync_service);
  ~PasswordFeatureManagerImpl() override = default;

  bool IsGenerationEnabled() const override;

  bool IsOptedInForAccountStorage() const override;
  bool ShouldShowAccountStorageOptIn() const override;
  bool ShouldShowAccountStorageReSignin(
      const GURL& current_page_url) const override;
  void OptInToAccountStorage() override;
  void OptOutOfAccountStorageAndClearSettings() override;

  bool ShouldShowAccountStorageBubbleUi() const override;

  void SetDefaultPasswordStore(const PasswordForm::Store& store) override;
  PasswordForm::Store GetDefaultPasswordStore() const override;
  metrics_util::PasswordAccountStorageUsageLevel
  ComputePasswordAccountStorageUsageLevel() const override;

  void RecordMoveOfferedToNonOptedInUser() override;
  int GetMoveOfferedToNonOptedInUserCount() const override;

 private:
  PrefService* const pref_service_;
  const syncer::SyncService* const sync_service_;
  DISALLOW_COPY_AND_ASSIGN(PasswordFeatureManagerImpl);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_IMPL_H_
