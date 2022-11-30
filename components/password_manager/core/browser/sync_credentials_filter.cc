// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_credentials_filter.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace password_manager {

SyncCredentialsFilter::SyncCredentialsFilter(
    PasswordManagerClient* client,
    SyncServiceFactoryFunction sync_service_factory_function)
    : client_(client),
      sync_service_factory_function_(std::move(sync_service_factory_function)) {
}

SyncCredentialsFilter::~SyncCredentialsFilter() = default;

bool SyncCredentialsFilter::ShouldSave(const PasswordForm& form) const {
  if (client_->IsIncognito())
    return false;

  if (form.form_data.is_gaia_with_skip_save_password_form)
    return false;

  const syncer::SyncService* sync_service =
      sync_service_factory_function_.Run();
  const signin::IdentityManager* identity_manager =
      client_->GetIdentityManager();

  if (base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage)) {
    // If kEnablePasswordsAccountStorage is enabled, then don't allow saving the
    // password if it corresponds to the primary account. Note that if the user
    // is just signing in to the first Gaia account, then IdentityManager might
    // not know about the account yet.
    if (sync_util::IsGaiaCredentialPage(form.signon_realm)) {
      CoreAccountInfo primary_account = identity_manager->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
      if (primary_account.IsEmpty() ||
          gaia::AreEmailsSame(base::UTF16ToUTF8(form.username_value),
                              primary_account.email)) {
        return false;
      }
    }
  } else {
    // If kEnablePasswordsAccountStorage is NOT enabled, then don't allow saving
    // the password for the sync account specifically.
    if (sync_util::IsSyncAccountCredential(form.url, form.username_value,
                                           sync_service, identity_manager)) {
      return false;
    }
  }

  return true;
}

bool SyncCredentialsFilter::ShouldSaveGaiaPasswordHash(
    const PasswordForm& form) const {
  if (base::FeatureList::IsEnabled(features::kPasswordReuseDetectionEnabled)) {
    return !client_->IsIncognito() &&
           sync_util::IsGaiaCredentialPage(form.signon_realm);
  }
  return false;
}

bool SyncCredentialsFilter::ShouldSaveEnterprisePasswordHash(
    const PasswordForm& form) const {
  return !client_->IsIncognito() && sync_util::ShouldSaveEnterprisePasswordHash(
                                        form, *client_->GetPrefs());
}

bool SyncCredentialsFilter::IsSyncAccountEmail(
    const std::string& username) const {
  return sync_util::IsSyncAccountEmail(username, client_->GetIdentityManager());
}

void SyncCredentialsFilter::ReportFormLoginSuccess(
    const PasswordFormManager& form_manager) const {
  const PasswordForm& form = form_manager.GetPendingCredentials();
  if (!form_manager.IsNewLogin() &&
      sync_util::IsSyncAccountCredential(form.url, form.username_value,
                                         sync_service_factory_function_.Run(),
                                         client_->GetIdentityManager())) {
    base::RecordAction(base::UserMetricsAction(
        "PasswordManager_SyncCredentialFilledAndLoginSuccessfull"));
  }
}

}  // namespace password_manager
