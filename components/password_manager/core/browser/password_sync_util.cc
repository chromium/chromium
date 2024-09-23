// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/origin.h"

using url::Origin;

namespace {

constexpr char kGoogleChangePasswordSignonRealm[] =
    "https://myaccount.google.com/";

}  // namespace

namespace password_manager {
namespace sync_util {

std::string GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
    const syncer::SyncService* sync_service) {
  // Return early if sync-the-feature isn't turned on or if the user explicitly
  // disabled password sync.
  if (!IsSyncFeatureEnabledIncludingPasswords(sync_service)) {
    return std::string();
  }

  return sync_service->GetAccountInfo().email;
}

bool IsSyncAccountEmail(const std::string& username,
                        const signin::IdentityManager* identity_manager,
                        signin::ConsentLevel consent_level) {
  // |identity_manager| can be null if user is not signed in.
  if (!identity_manager) {
    return false;
  }

  std::string sync_email =
      identity_manager->GetPrimaryAccountInfo(consent_level).email;

  if (sync_email.empty() || username.empty()) {
    return false;
  }

  if (username.find('@') == std::string::npos) {
    return false;
  }

  return gaia::AreEmailsSame(username, sync_email);
}

bool IsGaiaCredentialPage(const std::string& signon_realm) {
  const GURL signon_realm_url = GURL(signon_realm);
  const GURL gaia_signon_realm_url =
      GaiaUrls::GetInstance()->gaia_origin().GetURL();
  return signon_realm_url == gaia_signon_realm_url ||
         signon_realm_url == GURL(kGoogleChangePasswordSignonRealm);
}

bool ShouldSaveEnterprisePasswordHash(const PasswordForm& form,
                                      const PrefService& prefs) {
  if (base::FeatureList::IsEnabled(features::kPasswordReuseDetectionEnabled)) {
    return safe_browsing::MatchesPasswordProtectionLoginURL(form.url, prefs) ||
           safe_browsing::MatchesPasswordProtectionChangePasswordURL(form.url,
                                                                     prefs);
  }
  return false;
}

bool HasChosenToSyncPasswords(const syncer::SyncService* sync_service) {
  return sync_service && sync_service->GetDisableReasons().empty() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords);
}

bool IsSyncFeatureEnabledIncludingPasswords(
    const syncer::SyncService* sync_service) {
  // TODO(crbug.com/40066949): Remove this function once IsSyncFeatureEnabled()
  // is fully deprecated, see ConsentLevel::kSync documentation for details.
  return sync_service && sync_service->IsSyncFeatureEnabled() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords);
}

bool IsSyncFeatureActiveIncludingPasswords(
    const syncer::SyncService* sync_service) {
  return IsSyncFeatureEnabledIncludingPasswords(sync_service) &&
         sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS);
}

std::optional<std::string> GetAccountForSaving(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  if (!sync_service) {
    return std::nullopt;
  }
  if (IsSyncFeatureEnabledIncludingPasswords(sync_service) ||
      features_util::IsOptedInForAccountStorage(pref_service, sync_service)) {
    return sync_service->GetAccountInfo().email;
  }
  return std::nullopt;
}

SyncState GetPasswordSyncState(const syncer::SyncService* sync_service) {
  if (!sync_service ||
      !sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return SyncState::kNotActive;
  }

  return sync_service->GetUserSettings()->IsUsingExplicitPassphrase()
             ? SyncState::kActiveWithCustomPassphrase
             : SyncState::kActiveWithNormalEncryption;
}

}  // namespace sync_util
}  // namespace password_manager
