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

  // Note on password-account-storage methods on desktop vs mobile:
  // On desktop, there is an explicit per-user opt-in, and various associated
  // settings (e.g. which store is the default). On mobile, there is no explicit
  // opt-in, and no per-user settings.
  // As a consequence, all the corresponding setters (opting in/out, setting the
  // default store, etc) only exist on desktop. The getters exist on mobile too,
  // but have different (usually simpler) implementation.

  // Whether the current signed-in user (aka unconsented primary account) has
  // opted in to use the Google account storage for passwords (as opposed to
  // local/profile storage).
  virtual bool IsOptedInForAccountStorage() const = 0;

  // Returns the default storage location for signed-in but non-syncing users
  // (i.e. will new passwords be saved to locally or to the account by default).
  // Always returns an actual value, never kNotSet.
  virtual PasswordForm::Store GetDefaultPasswordStore() const = 0;

  // Returns whether the default storage location for newly-saved passwords is
  // explicitly set, i.e. whether the user has made an explicit choice where to
  // save. This can be used to detect "new" users, i.e. those that have never
  // interacted with an account-storage-enabled Save flow yet.
  virtual bool IsDefaultPasswordStoreSet() const = 0;

  // Returns the "usage level" of the account-scoped password storage. See
  // definition of PasswordAccountStorageUsageLevel.
  virtual features_util::PasswordAccountStorageUsageLevel
  ComputePasswordAccountStorageUsageLevel() const = 0;

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // Sets opt-in to using account storage for passwords for the current
  // signed-in user (unconsented primary account).
  virtual void OptInToAccountStorage() = 0;

  // Opts-out from using account storage for passwords for the
  // current signed-in user (unconsented primary account). Addditionally it sets
  // the default password store to kProfileStore.
  virtual void OptOutOfAccountStorage() = 0;

  // Sets the default password store selected by user in prefs. This store is
  // used for saving new credentials and adding blacking listing entries.
  virtual void SetDefaultPasswordStore(const PasswordForm::Store& store) = 0;

  // Whether the default store value should be changed to match the account
  // store setting. This is used to migrate users from having different
  // `GetDefaultPasswordStore` and `IsOptedInForAccountStorage` values.
  virtual bool ShouldChangeDefaultPasswordStore() const = 0;

#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  // Returns whether it is required to update the GMSCore based on the
  // GMSCore version.
  virtual bool ShouldUpdateGmsCore() = 0;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_H_
