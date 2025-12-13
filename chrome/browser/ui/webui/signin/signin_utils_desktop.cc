// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/sync/base/features.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"

SigninUIError CanOfferSignin(Profile* profile,
                             const GaiaId& gaia_id,
                             const std::string& email,
                             bool allow_account_from_other_profile) {
  if (!profile) {
    return SigninUIError::Other(email);
  }

  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return SigninUIError::Other(email);
  }

  if (!ChromeSigninClient::ProfileAllowsSigninCookies(profile)) {
    return SigninUIError::Other(email);
  }

  if (!email.empty()) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    if (!identity_manager) {
      return SigninUIError::Other(email);
    }

    // Make sure this username is not prohibited by policy.
    if (!signin::IsUsernameAllowedByPatternFromPrefs(
            g_browser_process->local_state(), email)) {
      return SigninUIError::UsernameNotAllowedByPatternFromPrefs(email);
    }

    // If the identity manager already has a primary account, then this is a
    // re-auth scenario. Make sure the email just signed in corresponds to
    // the one sign in manager expects.
    std::string current_email =
        identity_manager
            ->GetPrimaryAccountInfo(
                base::FeatureList::IsEnabled(
                    syncer::kReplaceSyncPromosWithSignInPromos)
                    ? signin::ConsentLevel::kSignin
                    : signin::ConsentLevel::kSync)
            .email;
    // TODO(crbug.com/440302112): Consider checking for the gaia_id equality
    // instead of the email for reauth flow detection.
    const bool same_email = gaia::AreEmailsSame(current_email, email);
    if (!current_email.empty() && !same_email) {
      return SigninUIError::WrongReauthAccount(email, current_email);
    }

    allow_account_from_other_profile =
        allow_account_from_other_profile ||
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kBypassAccountAlreadyUsedByAnotherProfileCheck);
    // If some profile, not just the current one, is already connected to this
    // account, don't offer sign in.
    if (g_browser_process && !same_email && !allow_account_from_other_profile) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      if (profile_manager) {
        std::vector<ProfileAttributesEntry*> entries =
            profile_manager->GetProfileAttributesStorage()
                .GetAllProfilesAttributes();

        for (const ProfileAttributesEntry* entry : entries) {
          // Ignore omitted profiles (these are notably profiles being created
          // using the signed-in profile creation flow). This is motivated by
          // these profile hanging around until the next restart which could
          // block subsequent profile creation, resulting in
          // SigninUIError::AccountAlreadyUsedByAnotherProfile.
          // TODO(crbug.com/40176394): This opens the possibility for getting
          // into a state with 2 profiles syncing to the same account:
          //  - start creating a new profile and sign-in,
          //  - enabled sync for the same account in another (existing) profile,
          //  - finish the profile creation by consenting to sync.
          // Properly addressing this would require deleting profiles from
          // cancelled flow right away, returning an error here for omitted
          // profiles, and fix the code that switches to the other syncing
          // profile so that the profile creation flow window gets activated for
          // profiles being created (instead of opening a new window).
          if (entry->IsOmitted() || entry->GetPath() == profile->GetPath()) {
            continue;
          }
          // If the feature is disabled, the below check on GaiaId equality is
          // equivalent to checking if the user is signed in.
          if (!base::FeatureList::IsEnabled(
                  syncer::kReplaceSyncPromosWithSignInPromos)) {
            if (!entry->IsAuthenticated() && !entry->CanBeManaged()) {
              continue;
            }
          }
          if (gaia_id == entry->GetGAIAId()) {
            return SigninUIError::AccountAlreadyUsedByAnotherProfile(
                email, entry->GetPath());
          }
        }
      }
    }

    // With force sign in enabled, cross account sign in is not allowed.
    if (signin_util::IsForceSigninEnabled() &&
        IsCrossAccountError(profile, gaia_id)) {
      std::string last_email = profile->GetPrefs()->GetString(
          prefs::kGoogleServicesLastSyncingUsername);
      return SigninUIError::ProfileWasUsedByAnotherAccount(email, last_email);
    }
  }

  return SigninUIError::Ok();
}

bool IsCrossAccountError(Profile* profile, const GaiaId& gaia_id) {
  DCHECK(!gaia_id.empty());
  const GaiaId last_gaia_id(
      profile->GetPrefs()->GetString(prefs::kGoogleServicesLastSyncingGaiaId));
  return !last_gaia_id.empty() && gaia_id != last_gaia_id;
}
