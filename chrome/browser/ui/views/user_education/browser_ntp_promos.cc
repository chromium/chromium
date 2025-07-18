// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_ntp_promos.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/user_education/ntp_promo_identifiers.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/base/features.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_metadata.h"

using user_education::NtpPromoContent;
using user_education::NtpPromoSpecification;

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
      base::BindRepeating([](Profile* profile) {
        return NtpPromoSpecification::Eligibility::kEligible;
      }),
      /*action_callback=*/base::DoNothing(),
      /*show_after=*/{},
      user_education::Metadata(
          141, "cjgrant@google.com",
          "Promotes sign-in capability on the New Tab Page")));
}
