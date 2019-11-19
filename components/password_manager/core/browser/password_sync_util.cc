// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_user_settings.h"
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
    const signin::IdentityManager* identity_manager) {
  if (!identity_manager)
    return std::string();

  // Return early if the user has explicitly disabled password sync. Note that
  // this does not cover the case when sync as a whole is turned off.
  if (sync_service && !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                          syncer::UserSelectableType::kPasswords)) {
    return std::string();
  }

  return identity_manager->GetPrimaryAccountInfo().email;
}

bool IsSyncAccountCredential(const autofill::PasswordForm& form,
                             const syncer::SyncService* sync_service,
                             const signin::IdentityManager* identity_manager) {
  if (!GURL(form.signon_realm).DomainIs("google.com"))
    return false;

  // The empty username can mean that Chrome did not detect it correctly. For
  // reasons described in http://crbug.com/636292#c1, the username is suspected
  // to be the sync username unless proven otherwise.
  if (form.username_value.empty())
    return true;

  return gaia::AreEmailsSame(
      base::UTF16ToUTF8(form.username_value),
      GetSyncUsernameIfSyncingPasswords(sync_service, identity_manager));
}

bool IsSyncAccountEmail(const std::string& username,
                        const signin::IdentityManager* identity_manager) {
  // |identity_manager| can be null if user is not signed in.
  if (!identity_manager)
    return false;

  std::string sync_email = identity_manager->GetPrimaryAccountInfo().email;

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
