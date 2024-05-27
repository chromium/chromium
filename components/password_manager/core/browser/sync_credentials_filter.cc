// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_credentials_filter.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "password_sync_util.h"

namespace password_manager {

SyncCredentialsFilter::SyncCredentialsFilter(PasswordManagerClient* client)
    : client_(client) {}

SyncCredentialsFilter::~SyncCredentialsFilter() = default;

bool SyncCredentialsFilter::ShouldSave(const PasswordForm& form) const {
  if (client_->IsOffTheRecord()) {
    return false;
  }

  if (form.form_data.is_gaia_with_skip_save_password_form()) {
    return false;
  }

  if (!sync_util::IsGaiaCredentialPage(form.signon_realm)) {
    return true;
  }

  // Note that `sync_service` may be null in advanced cases like --disable-sync
  // being used as per syncer::IsSyncAllowedByFlag().
  const syncer::SyncService* sync_service = client_->GetSyncService();

  // The requirement to fulfill is "don't offer to save a Gaia password inside
  // its own account".
  // Let's assume that if the browser is signed-in, new passwords are saved to
  // the primary signed-in account. Per sync_util::GetAccountForSaving(), that's
  // not always true, but let's not overcomplicate.
  const CoreAccountInfo primary_account = sync_service != nullptr
                                              ? sync_service->GetAccountInfo()
                                              : CoreAccountInfo();
  if (!primary_account.IsEmpty()) {
    // Only save if the account is not the same. If the username is empty, in
    // doubt don't save (this is relevant in the password change page).
    return !form.username_value.empty() &&
           !gaia::AreEmailsSame(base::UTF16ToUTF8(form.username_value),
                                primary_account.email);
  }

// The browser is signed-out and the web just signed-in.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // On desktop, this normally leads to immediate browser sign-in, in which case
  // we shouldn't offer saving. One exception is if browser sign-in is disabled.
  return !client_->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
#else
  // On mobile, sign-in via the web page doesn't lead to browser sign-in, so
  // offer saving.
  // (Navigating to the Gaia web page opens Chrome UI which must be accepted to
  // perform browser+web sign-in. The code path here is only hit if that UI was
  // suppressed/ dismissed and the user interacted directly with the page.)
  return true;
#endif
}

bool SyncCredentialsFilter::ShouldSaveGaiaPasswordHash(
    const PasswordForm& form) const {
  if (base::FeatureList::IsEnabled(features::kPasswordReuseDetectionEnabled)) {
    return !client_->IsOffTheRecord() &&
           sync_util::IsGaiaCredentialPage(form.signon_realm);
  }
  return false;
}

bool SyncCredentialsFilter::ShouldSaveEnterprisePasswordHash(
    const PasswordForm& form) const {
  return !client_->IsOffTheRecord() &&
         sync_util::ShouldSaveEnterprisePasswordHash(form,
                                                     *client_->GetPrefs());
}

bool SyncCredentialsFilter::IsSyncAccountEmail(
    const std::string& username) const {
  // TODO(crbug.com/40067296): `signin::ConsentLevel::kSync` is
  // deprecated. Remove this usage.
  return sync_util::IsSyncAccountEmail(username, client_->GetIdentityManager(),
                                       signin::ConsentLevel::kSync);
}

}  // namespace password_manager
