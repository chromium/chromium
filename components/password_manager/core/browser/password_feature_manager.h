// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_H_

#include "build/build_config.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace password_manager {

// Keeps track of which feature of PasswordManager is enabled.
class PasswordFeatureManager {
 public:
  PasswordFeatureManager() = default;

  PasswordFeatureManager(const PasswordFeatureManager&) = delete;
  PasswordFeatureManager& operator=(const PasswordFeatureManager&) = delete;

  virtual ~PasswordFeatureManager() = default;

  virtual bool IsGenerationEnabled() const = 0;

  // Whether the user should be asked for authentication before filling
  // passwords. This is true for eligible users that have enabled this feature
  // before.
  virtual bool IsBiometricAuthenticationBeforeFillingEnabled() const = 0;

  // Whether the Google account storage for passwords is active for the current
  // signed-in user. This always returns false for sync-the-feature users and
  // signed out users. Account storage can be enabled/disabled via
  // syncer::SyncUserSettings::SetSelectedType().
  //
  // Note that "active" here is largely in line with Sync's definition: account
  // storage is enabled and there are no sync errors preventing password sync
  // from working. Thus, passwords saved in this state are very likely to be
  // synced to the Google account (barring unexpected errors). Sync's definition
  // of "active", however, is slightly stricter: During startup, while it's not
  // known whether a data type will encounter errors, it's not considered
  // active. This method assumes no errors in that case.
  //
  // Also note that sync-the-feature users might still sync passwords to the
  // Google account using the profile store.
  virtual bool IsAccountStorageActive() const = 0;

  // Returns the "usage level" of the account-scoped password storage. See
  // definition of PasswordAccountStorageUsageLevel.
  virtual features_util::PasswordAccountStorageUsageLevel
  ComputePasswordAccountStorageUsageLevel() const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_H_
