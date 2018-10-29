// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_credentials_filter.h"

#include <algorithm>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

// Returns true if the last loaded page was for transactional re-auth on a
// Google property.
bool LastLoadWasTransactionalReauthPage(const GURL& last_load_url) {
  if (last_load_url.GetOrigin() !=
      GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  // TODO(vabr): GAIA stops using the "rart" URL param, and instead includes a
  // hidden form field with name "rart". http://crbug.com/543085
  // "rart" is the transactional reauth paramter.
  std::string ignored_value;
  return net::GetValueForKeyInQuery(last_load_url, "rart", &ignored_value);
}

}  // namespace

SyncCredentialsFilter::SyncCredentialsFilter(
    const PasswordManagerClient* client,
    SyncServiceFactoryFunction sync_service_factory_function,
    SigninManagerFactoryFunction signin_manager_factory_function)
    : client_(client),
      sync_service_factory_function_(sync_service_factory_function),
      signin_manager_factory_function_(signin_manager_factory_function) {}

SyncCredentialsFilter::~SyncCredentialsFilter() {}

std::vector<std::unique_ptr<PasswordForm>> SyncCredentialsFilter::FilterResults(
    std::vector<std::unique_ptr<PasswordForm>> results) const {
  const AutofillForSyncCredentialsState autofill_sync_state =
      GetAutofillForSyncCredentialsState();

  if (autofill_sync_state != DISALLOW_SYNC_CREDENTIALS &&
      (autofill_sync_state != DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH ||
       !LastLoadWasTransactionalReauthPage(
           client_->GetLastCommittedEntryURL()))) {
    return results;
  }

  auto begin_of_removed =
      std::partition(results.begin(), results.end(),
                     [this](const std::unique_ptr<PasswordForm>& form) {
                       return ShouldSave(*form);
                     });

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.SyncCredentialFiltered",
                        begin_of_removed != results.end());

  results.erase(begin_of_removed, results.end());

  return results;
}

bool SyncCredentialsFilter::ShouldSave(
    const autofill::PasswordForm& form) const {
  return !client_->IsIncognito() &&
         !form.is_gaia_with_skip_save_password_form &&
         !sync_util::IsSyncAccountCredential(
             form, sync_service_factory_function_.Run(),
             signin_manager_factory_function_.Run());
}

bool SyncCredentialsFilter::ShouldSaveGaiaPasswordHash(
    const autofill::PasswordForm& form) const {
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  return !client_->IsIncognito() &&
         sync_util::IsGaiaCredentialPage(form.signon_realm);
#else
  return false;
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED
}

bool SyncCredentialsFilter::ShouldSaveEnterprisePasswordHash(
    const autofill::PasswordForm& form) const {
  return !client_->IsIncognito() && sync_util::ShouldSaveEnterprisePasswordHash(
                                        form, *client_->GetPrefs());
}

bool SyncCredentialsFilter::IsSyncAccountEmail(
    const std::string& username) const {
  return sync_util::IsSyncAccountEmail(username,
                                       signin_manager_factory_function_.Run());
}

void SyncCredentialsFilter::ReportFormLoginSuccess(
    const PasswordFormManagerInterface& form_manager) const {
  if (!form_manager.IsNewLogin() &&
      sync_util::IsSyncAccountCredential(
          form_manager.GetPendingCredentials(),
          sync_service_factory_function_.Run(),
          signin_manager_factory_function_.Run())) {
    base::RecordAction(base::UserMetricsAction(
        "PasswordManager_SyncCredentialFilledAndLoginSuccessfull"));
  }
}

// static
SyncCredentialsFilter::AutofillForSyncCredentialsState
SyncCredentialsFilter::GetAutofillForSyncCredentialsState() {
  bool protect_sync_credential_enabled =
      base::FeatureList::IsEnabled(features::kProtectSyncCredential);
  bool protect_sync_credential_on_reauth_enabled =
      base::FeatureList::IsEnabled(features::kProtectSyncCredentialOnReauth);

  if (protect_sync_credential_enabled) {
    if (protect_sync_credential_on_reauth_enabled) {
      // Both the features are enabled, do not ever fill the sync credential.
      return DISALLOW_SYNC_CREDENTIALS;
    }

    // Only 'ProtectSyncCredentialOnReauth' feature is kept disabled. This
    // is "illegal", emit a warning and do not ever fill the sync credential.
    LOG(WARNING) << "This is illegal! Feature "
                    "'ProtectSyncCredentialOnReauth' cannot be kept "
                    "disabled if 'protect-sync-credential' feature is enabled. "
                    "We shall not ever fill the sync credential is such cases.";
    return DISALLOW_SYNC_CREDENTIALS;
  }

  if (protect_sync_credential_on_reauth_enabled) {
    // Only 'ProtectSyncCredentialOnReauth' feature is kept enabled, fill
    // the sync credential everywhere but on reauth.
    return DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH;
  }

  // Both the features are disabled, fill the sync credential everywhere.
  return ALLOW_SYNC_CREDENTIALS;
}

}  // namespace password_manager
