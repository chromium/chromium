// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace features {

// Features

// Controls whether or not Autofill client will populate form with CPAN and
// dCVV, rather than FPAN.
const base::Feature kAutofillAlwaysReturnCloudTokenizedCard{
    "AutofillAlwaysReturnCloudTokenizedCard",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, manual fallback will be auto-triggered on form interaction in
// the case where autofill failed to fill a credit card form accurately.
const base::Feature kAutofillAutoTriggerManualFallbackForCards{
    "AutofillAutoTriggerManualFallbackForCards",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of platform authenticators through WebAuthn to retrieve
// credit cards from Google payments.
const base::Feature kAutofillCreditCardAuthentication{
  "AutofillCreditCardAuthentication",
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_ANDROID)
      // Better Auth project is fully launched on Win/Mac/Clank.
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// When enabled, if credit card upload succeeded, the avatar icon will show a
// highlight otherwise, the credit card icon image will be updated and if user
// clicks on the icon, a save card failure bubble will pop up.
const base::Feature kAutofillCreditCardUploadFeedback{
    "AutofillCreditCardUploadFeedback", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we show a Google-issued card in the suggestions list.
const base::Feature kAutofillEnableGoogleIssuedCard{
    "AutofillEnableGoogleIssuedCard", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, merchant bound virtual cards will be offered when users
// interact with a payment form.
const base::Feature kAutofillEnableMerchantBoundVirtualCards{
    "AutofillEnableMerchantBoundVirtualCards",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to track the cross-tab-status of the offer notification
// bubble.
const base::Feature kAutofillEnableOfferNotificationCrossTabTracking{
    "AutofillEnableOfferNotificationCrossTabTracking",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, a notification will be displayed on page navigation if the
// domain has an eligible merchant promo code offer or reward.
const base::Feature kAutofillEnableOfferNotificationForPromoCodes{
    "AutofillEnableOfferNotificationForPromoCodes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, offers will be displayed in the Clank keyboard accessory during
// downstream.
const base::Feature kAutofillEnableOffersInClankKeyboardAccessory{
    "AutofillEnableOffersInClankKeyboardAccessory",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, offer data will be retrieved during downstream and shown in
// the dropdown list.
const base::Feature kAutofillEnableOffersInDownstream{
    "kAutofillEnableOffersInDownstream", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, if the user interacts with the manual fallback bottom sheet
// on Android, it'll remain sticky until the user dismisses it.
const base::Feature kAutofillEnableStickyManualFallbackForCards{
    "AutofillEnableStickyManualFallbackForCards",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill data related icons will be shown in the status
// chip in toolbar along with the avatar toolbar button.
const base::Feature kAutofillEnableToolbarStatusChip{
    "AutofillEnableToolbarStatusChip", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the option of using cloud token virtual card will be offered
// when all requirements are met.
const base::Feature kAutofillEnableVirtualCard{
    "AutofillEnableVirtualCard", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, virtual card retrieval will pass an optional
// authentication based on risk level.
const base::Feature kAutofillEnableVirtualCardsRiskBasedAuthentication{
    "AutofillEnableVirtualCardsRiskBasedAuthentication",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will attempt to fill merchant promo/coupon/gift code
// fields when data is available.
const base::Feature kAutofillFillMerchantPromoCodeFields{
    "AutofillFillMerchantPromoCodeFields", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable the fix for the offer feature in Incognito mode.
const base::Feature kAutofillFixOfferInIncognito{
    "AutofillFixOfferInIncognito", base::FEATURE_DISABLED_BY_DEFAULT};

// The merchant bound virtual card feature introduces new customized card art
// images. This parameter defines the expiration of the fetched image in the
// disk cache of the image fetcher.
const base::FeatureParam<int> kAutofillImageFetcherDiskCacheExpirationInMinutes{
    &kAutofillEnableMerchantBoundVirtualCards,
    "autofill_image_fetcher_disk_cache_expiration_in_minutes", 10};

// When enabled, Autofill will attempt to find merchant promo/coupon/gift code
// fields when parsing forms.
const base::Feature kAutofillParseMerchantPromoCodeFields{
    "AutofillParseMerchantPromoCodeFields", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the Save Card infobar will be dismissed by a user initiated
// navigation other than one caused by submitted form.
const base::Feature kAutofillSaveCardDismissOnNavigation{
    "AutofillSaveCardDismissOnNavigation", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the Save Card infobar supports editing before submitting.
const base::Feature kAutofillSaveCardInfobarEditSupport{
    "AutofillSaveCardInfobarEditSupport", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the entire PAN and the CVC details of the unmasked cached card
// will be shown in the manual filling view.
const base::Feature kAutofillShowUnmaskedCachedCardInManualFillingView{
    "AutofillShowUnmaskedCachedCardInManualFillingView",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, suggestions with offers will be shown at the top.
const base::Feature kAutofillSortSuggestionsBasedOnOfferPresence{
    "AutofillSortSuggestionsBasedOnOfferPresence",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, merchant bound virtual cards will be suggested even if we don't
// detect all of the card number, exp date and CVC fields in the payment form.
const base::Feature kAutofillSuggestVirtualCardsOnIncompleteForm{
    "AutofillSuggestVirtualCardsOnIncompleteForm",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because the feature state depends on the user's country.
// There are countries we simply can't turn this on for, and they change over
// time, so it's important that we can flip a switch and be done instead of
// having old versions of Chrome forever do the wrong thing. Enabling it by
// default would mean that any first-run client without a Finch config won't get
// the overriding command to NOT turn it on, which becomes an issue.
const base::Feature kAutofillUpstream{"AutofillUpstream",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamAllowAllEmailDomains{
    "AutofillUpstreamAllowAllEmailDomains", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldShowImprovedUserConsentForCreditCardSave() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_APPLE) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // The new user consent UI is fully launched on MacOS, Windows and Linux.
  return true;
#else
  // Chrome OS does not have the new UI.
  return false;
#endif
}

}  // namespace features
}  // namespace autofill
