// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/origin.h"

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

using autofill::PasswordForm;
using url::Origin;

namespace {

constexpr char kGoogleChangePasswordSignonRealm[] =
    "https://myaccount.google.com/";

}  // namespace

namespace password_manager {
namespace sync_util {

std::string GetSyncUsernameIfSyncingPasswords(
    const syncer::SyncService* sync_service,
    const SigninManagerBase* signin_manager) {
  if (!signin_manager)
    return std::string();

  // If sync is set up, return early if we aren't syncing passwords.
  if (sync_service &&
      !sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS)) {
    return std::string();
  }

  return signin_manager->GetAuthenticatedAccountInfo().email;
}

bool IsSyncAccountCredential(const autofill::PasswordForm& form,
                             const syncer::SyncService* sync_service,
                             const SigninManagerBase* signin_manager) {
  if (!IsGaiaCredentialPage(form.signon_realm))
    return false;

  // The empty username can mean that Chrome did not detect it correctly. For
  // reasons described in http://crbug.com/636292#c1, the username is suspected
  // to be the sync username unless proven otherwise.
  if (form.username_value.empty())
    return true;

  return gaia::AreEmailsSame(
      base::UTF16ToUTF8(form.username_value),
      GetSyncUsernameIfSyncingPasswords(sync_service, signin_manager));
}

bool ShouldSavePasswordHash(const autofill::PasswordForm& form,
                            const SigninManagerBase* signin_manager,
                            PrefService* prefs) {
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  bool is_protected_credential_url =
      gaia::IsGaiaSignonRealm(GURL(form.signon_realm)) ||
      form.signon_realm == kGoogleChangePasswordSignonRealm ||
      safe_browsing::MatchesPasswordProtectionLoginURL(form.origin, *prefs) ||
      safe_browsing::MatchesPasswordProtectionChangePasswordURL(form.origin,
                                                                *prefs);

  if (!is_protected_credential_url)
    return false;

  std::string sync_email = signin_manager->GetAuthenticatedAccountInfo().email;
  std::string username = base::UTF16ToUTF8(form.username_value);

  if (sync_email.empty() || username.empty())
    return false;

  // Add @domain.name to the username if it is absent.
  std::string email =
      username + (username.find('@') == std::string::npos
                      ? "@" + gaia::ExtractDomainName(sync_email)
                      : std::string());

  return email == sync_email;
#else
  return false;
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED
}

bool IsSyncAccountEmail(const std::string& username,
                        const SigninManagerBase* signin_manager) {
  // |signin_manager| can be null if user is not signed in.
  if (!signin_manager)
    return false;

  std::string sync_email = signin_manager->GetAuthenticatedAccountInfo().email;

  if (sync_email.empty() || username.empty())
    return false;

  if (username.find('@') == std::string::npos)
    return false;

  return gaia::AreEmailsSame(username, sync_email);
}

bool IsGaiaCredentialPage(const std::string& signon_realm) {
  return gaia::IsGaiaSignonRealm(GURL(signon_realm)) ||
         signon_realm == kGoogleChangePasswordSignonRealm;
}

bool ShouldSaveEnterprisePasswordHash(const autofill::PasswordForm& form,
                                      const PrefService& prefs) {
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  return safe_browsing::MatchesPasswordProtectionLoginURL(form.origin, prefs) ||
         safe_browsing::MatchesPasswordProtectionChangePasswordURL(form.origin,
                                                                   prefs);
#else
  return false;
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED
}

}  // namespace sync_util
}  // namespace password_manager
