// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/investigator_dependency_provider.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

bool CanOfferSignin(Profile* profile,
                    CanOfferSigninType can_offer,
                    const std::string& gaia_id,
                    const std::string& email,
                    std::string* error_message) {
  if (error_message)
    error_message->clear();

  if (!profile)
    return false;

  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed))
    return false;

  if (!ChromeSigninClient::ProfileAllowsSigninCookies(profile))
    return false;

  if (!email.empty()) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    if (!identity_manager)
      return false;

    // Make sure this username is not prohibited by policy.
    if (!signin::IsUsernameAllowedByPatternFromPrefs(
            g_browser_process->local_state(), email)) {
      if (error_message) {
        error_message->assign(
            l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED));
      }
      return false;
    }

    if (can_offer == CAN_OFFER_SIGNIN_FOR_SECONDARY_ACCOUNT)
      return true;

    // If the identity manager already has a primary account, then this is a
    // re-auth scenario.  Make sure the email just signed in corresponds to
    // the one sign in manager expects.
    std::string current_email = identity_manager->GetPrimaryAccountInfo().email;
    const bool same_email = gaia::AreEmailsSame(current_email, email);
    if (!current_email.empty() && !same_email) {
      UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                                signin_metrics::HISTOGRAM_ACCOUNT_MISSMATCH,
                                signin_metrics::HISTOGRAM_REAUTH_MAX);
      if (error_message) {
        error_message->assign(l10n_util::GetStringFUTF8(
            IDS_SYNC_WRONG_EMAIL, base::UTF8ToUTF16(current_email)));
      }
      return false;
    }

    // If some profile, not just the current one, is already connected to this
    // account, don't show the infobar.
    if (g_browser_process && !same_email) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      if (profile_manager) {
        std::vector<ProfileAttributesEntry*> entries =
            profile_manager->GetProfileAttributesStorage()
                .GetAllProfilesAttributes();

        for (const ProfileAttributesEntry* entry : entries) {
          if (!entry->IsAuthenticated())
            continue;

          // For backward compatibility, need to check also the username of the
          // profile, since the GAIA ID may not have been set yet in the
          // ProfileAttributesStorage.  It will be set once the profile
          // is opened.
          std::string profile_gaia_id = entry->GetGAIAId();
          std::string profile_email = base::UTF16ToUTF8(entry->GetUserName());
          if (gaia_id == profile_gaia_id ||
              gaia::AreEmailsSame(email, profile_email)) {
            if (error_message) {
              error_message->assign(
                  l10n_util::GetStringUTF8(IDS_SYNC_USER_NAME_IN_USE_ERROR));
            }
            return false;
          }
        }
      }
    }

    // With force sign in enabled, cross account sign in is not allowed.
    if (signin_util::IsForceSigninEnabled() &&
        IsCrossAccountError(profile, email, gaia_id)) {
      if (error_message) {
        std::string last_email =
            profile->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
        error_message->assign(l10n_util::GetStringFUTF8(
            IDS_SYNC_USED_PROFILE_ERROR, base::UTF8ToUTF16(last_email)));
      }
      return false;
    }
  }

  return true;
}

bool IsCrossAccountError(Profile* profile,
                         const std::string& email,
                         const std::string& gaia_id) {
  InvestigatorDependencyProvider provider(profile);
  InvestigatedScenario scenario =
      SigninInvestigator(email, gaia_id, &provider).Investigate();

  return scenario == InvestigatedScenario::kDifferentAccount;
}
