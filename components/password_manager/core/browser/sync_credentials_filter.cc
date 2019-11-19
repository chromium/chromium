// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_credentials_filter.h"

#include <algorithm>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

using autofill::PasswordForm;

namespace password_manager {

SyncCredentialsFilter::SyncCredentialsFilter(
    PasswordManagerClient* client,
    SyncServiceFactoryFunction sync_service_factory_function)
    : client_(client),
      sync_service_factory_function_(std::move(sync_service_factory_function)) {
}

SyncCredentialsFilter::~SyncCredentialsFilter() {}

bool SyncCredentialsFilter::ShouldSave(
    const autofill::PasswordForm& form) const {
  return !client_->IsIncognito() &&
         !form.form_data.is_gaia_with_skip_save_password_form &&
         !sync_util::IsSyncAccountCredential(
             form, sync_service_factory_function_.Run(),
             client_->GetIdentityManager());
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
  return sync_util::IsSyncAccountEmail(username, client_->GetIdentityManager());
}

void SyncCredentialsFilter::ReportFormLoginSuccess(
    const PasswordFormManager& form_manager) const {
  if (!form_manager.IsNewLogin() &&
      sync_util::IsSyncAccountCredential(form_manager.GetPendingCredentials(),
                                         sync_service_factory_function_.Run(),
                                         client_->GetIdentityManager())) {
    base::RecordAction(base::UserMetricsAction(
        "PasswordManager_SyncCredentialFilledAndLoginSuccessfull"));
  }
}

}  // namespace password_manager
