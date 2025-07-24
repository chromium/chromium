// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_ntp_promos.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/user_education/ntp_promo_identifiers.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_metadata.h"

using user_education::NtpPromoContent;
using user_education::NtpPromoSpecification;

namespace {

NtpPromoSpecification::Eligibility CheckSignInPromoEligibility(
    Profile* profile) {
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return NtpPromoSpecification::Eligibility::kIneligible;
  }

  const auto signed_in_state = signin_util::GetSignedInState(
      IdentityManagerFactory::GetForProfile(profile));
  switch (signed_in_state) {
    case signin_util::SignedInState::kSignedOut:
      // User is fully signed out.
      return NtpPromoSpecification::Eligibility::kEligible;
    case signin_util::SignedInState::kWebOnlySignedIn:
      // When signed in on the web, one-click sign in options exist elsewhere
      // in Chrome. This promo currently only offers the full-sign-in flow, so
      // don't show it to users already signed in on the Web.
      return NtpPromoSpecification::Eligibility::kIneligible;
    case signin_util::SignedInState::kSignedIn:
    case signin_util::SignedInState::kSyncing:
    case signin_util::SignedInState::kSignInPending:
    case signin_util::SignedInState::kSyncPaused:
      // All other cases are considered completed.
      return NtpPromoSpecification::Eligibility::kCompleted;
  }
}

void InvokeSignInPromo(BrowserWindowInterface* browser) {
  // Note that this invokes a "from scratch" sign-in flow, even if the user is
  // already signed in on the Web. Later, we can evolve this if desired to
  // offer an alternate one-click sign-in flow for those other users.
  signin_ui_util::ShowSigninPromptFromPromo(
      browser->GetProfile(), signin_metrics::AccessPoint::kNtpFeaturePromo);
}

}  // namespace

void MaybeRegisterNtpPromos(user_education::NtpPromoRegistry& registry) {
  if (registry.AreAnyPromosRegistered()) {
    return;
  }

  // Register NTP Promos below.
  //
  // Absent MRU/LRU and explicit `show_after` parameters, promos will be shown
  // in the order they appear here, so pay careful attention to what order new
  // users should see promos in (especially as not all promos may be able to
  // display at once).
  //
  // NOTE: Changes to this file should be reviewed by both a User Education
  // owner (//components/user_education/OWNERS) and an NTP owner
  // (//components/search/OWNERS).

  registry.AddPromo(NtpPromoSpecification(
      kNtpSignInPromoId,
      NtpPromoContent("chrome-filled",
                      base::FeatureList::IsEnabled(
                          syncer::kReplaceSyncPromosWithSignInPromos)
                          ? IDS_NTP_SIGN_IN_PROMO_WITH_BOOKMARKS
                          : IDS_NTP_SIGN_IN_PROMO,
                      IDS_NTP_SIGN_IN_PROMO_ACTION_BUTTON),
      base::BindRepeating(&CheckSignInPromoEligibility),
      base::BindRepeating(&InvokeSignInPromo),
      /*show_after=*/{},
      user_education::Metadata(
          141, "cjgrant@google.com",
          "Promotes sign-in capability on the New Tab Page")));
}
