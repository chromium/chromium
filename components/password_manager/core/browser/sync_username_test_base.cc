// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_username_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/browser/signin_pref_names.h"

using autofill::PasswordForm;

namespace password_manager {

SyncUsernameTestBase::LocalFakeSyncService::LocalFakeSyncService()
    : syncing_passwords_(true) {}

SyncUsernameTestBase::LocalFakeSyncService::~LocalFakeSyncService() {}

syncer::ModelTypeSet
SyncUsernameTestBase::LocalFakeSyncService::GetPreferredDataTypes() const {
  if (syncing_passwords_)
    return syncer::ModelTypeSet(syncer::PASSWORDS);
  return syncer::ModelTypeSet();
}

SyncUsernameTestBase::SyncUsernameTestBase()
    : signin_client_(&prefs_),
#if defined(OS_CHROMEOS)
      signin_manager_(&signin_client_,
                      &account_tracker_,
                      nullptr /* signin_error_controller */) {
#else
      token_service_(&prefs_),
      signin_manager_(&signin_client_,
                      &token_service_,
                      &account_tracker_,
                      nullptr, /* cookie_manager_service */
                      nullptr, /* signin_error_controller */
                      signin::AccountConsistencyMethod::kDisabled) {
#endif
  SigninManagerBase::RegisterProfilePrefs(prefs_.registry());
  AccountTrackerService::RegisterPrefs(prefs_.registry());
#if !defined(OS_CHROMEOS)
  ProfileOAuth2TokenService::RegisterProfilePrefs(prefs_.registry());
#endif
  account_tracker_.Initialize(&prefs_, base::FilePath());
}

SyncUsernameTestBase::~SyncUsernameTestBase() {}

void SyncUsernameTestBase::FakeSigninAs(const std::string& email) {
  signin_manager_.SetAuthenticatedAccountInfo("12345", email);
}

// static
PasswordForm SyncUsernameTestBase::SimpleGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://accounts.google.com";
  form.username_value = base::ASCIIToUTF16(username);
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.username_value = base::ASCIIToUTF16(username);
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username,
                                                     const char* origin) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.username_value = base::ASCIIToUTF16(username);
  form.origin = GURL(origin);
  return form;
}

void SyncUsernameTestBase::SetSyncingPasswords(bool syncing_passwords) {
  sync_service_.set_syncing_passwords(syncing_passwords);
}

}  // namespace password_manager
