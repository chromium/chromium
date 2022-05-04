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

namespace autofill::features {

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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
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

// When enabled, merchant bound virtual cards will be offered when users
// interact with a payment form.
const base::Feature kAutofillEnableMerchantBoundVirtualCards{
    "AutofillEnableMerchantBoundVirtualCards",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, enable manual falling component for virtual cards on Android.
const base::Feature kAutofillEnableManualFallbackForVirtualCards{
    "AutofillEnableManualFallbackForVirtualCards",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, a notification will be displayed on page navigation if the
// domain has an eligible merchant promo code offer or reward.
const base::Feature kAutofillEnableOfferNotificationForPromoCodes{
    "AutofillEnableOfferNotificationForPromoCodes",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, offers will be displayed in the Clank keyboard accessory during
// downstream.
const base::Feature kAutofillEnableOffersInClankKeyboardAccessory{
    "AutofillEnableOffersInClankKeyboardAccessory",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we send billing customer number in GetUploadDetails
// preflight call.
const base::Feature kAutofillEnableSendingBcnInGetUploadDetails{
    "AutofillEnableSendingBcnInGetUploadDetails",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, if the user interacts with the manual fallback bottom sheet
// on Android, it'll remain sticky until the user dismisses it.
const base::Feature kAutofillEnableStickyManualFallbackForCards{
    "AutofillEnableStickyManualFallbackForCards",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill data related icons will be shown in the status
// chip in toolbar along with the avatar toolbar button.
const base::Feature kAutofillEnableToolbarStatusChip{
    "AutofillEnableToolbarStatusChip", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, UnmaskCardRequest will set instrument id, which is Chrome-side
// field for non-legacy ID.
const base::Feature kAutofillEnableUnmaskCardRequestSetInstrumentId{
    "AutofillEnableUnmaskCardRequestSetInstrumentId",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the user will have the ability to update the virtual card
// enrollment of a credit card through their chrome browser after certain
// autofill flows (for example, downstream and upstream), and from the settings
// page.
const base::Feature kAutofillEnableUpdateVirtualCardEnrollment{
    "AutofillEnableUpdateVirtualCardEnrollment",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the option of using cloud token virtual card will be offered
// when all requirements are met.
const base::Feature kAutofillEnableVirtualCard{
    "AutofillEnableVirtualCard", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, after a successful authentication to autofill a virtual card,
// the user will be prompted to opt-in to FIDO if the user is not currently
// opted-in, and if the user is opted-in already and the virtual card is FIDO
// eligible the user will be prompted to register the virtual card into FIDO.
const base::Feature kAutofillEnableVirtualCardFidoEnrollment(
    "AutofillEnableVirtualCardFidoEnrollment",
    base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, in the payments settings page on desktop, virtual card
// enrollment management will be provided so that the user can enroll/unenroll a
// card in virtual card.
const base::Feature kAutofillEnableVirtualCardManagementInDesktopSettingsPage{
    "AutofillEnableVirtualCardManagementInDesktopSettingsPage",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, Chrome will show metadata along with other card information
// when the virtual card is presented to users.
const base::Feature kAutofillEnableVirtualCardMetadata{
    "AutofillEnableVirtualCardMetadata", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, virtual card retrieval will pass an optional
// authentication based on risk level.
const base::Feature kAutofillEnableVirtualCardsRiskBasedAuthentication{
    "AutofillEnableVirtualCardsRiskBasedAuthentication",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, if the previous feature offer was declined, a delay will be
// added before Chrome attempts to show offer again.
const base::Feature kAutofillEnforceDelaysInStrikeDatabase{
    "AutofillEnforceDelaysInStrikeDatabase", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will attempt to fill merchant promo/coupon/gift code
// fields when data is available.
const base::Feature kAutofillFillMerchantPromoCodeFields{
    "AutofillFillMerchantPromoCodeFields", base::FEATURE_DISABLED_BY_DEFAULT};

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

// When enabled, Chrome will display experimental UI variants to the user
// during the upload save card process.
const base::Feature kAutofillSaveCardUiExperiment{
    "AutofillSaveCardUiExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// This will select one of the options for the save card UI bubble which we
// want to display to the user. The value will be an integer(number).
const base::FeatureParam<int> kAutofillSaveCardUiExperimentSelectorInNumber{
    &kAutofillSaveCardUiExperiment,
    "autofill_save_card_ui_experiment_selector_in_number", 0};

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
// The set of launched countries is listed in autofill_experiments.cc, and this
// flag remains as a way to easily enable upload credit card save for testers,
// as well as enable non-fully-launched countries on a trial basis.
const base::Feature kAutofillUpstream{"AutofillUpstream",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Chrome allows credit card upload to Google Payments if the
// user's email domain is from a common email provider (thus unlikely to be an
// enterprise or education user).
const base::Feature kAutofillUpstreamAllowAdditionalEmailDomains{
    "AutofillUpstreamAllowAdditionalEmailDomains",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Chrome allows credit card upload to Google Payments, no matter
// the user's email domain.
const base::Feature kAutofillUpstreamAllowAllEmailDomains{
    "AutofillUpstreamAllowAllEmailDomains", base::FEATURE_DISABLED_BY_DEFAULT};

// The delay required since the last strike before offering another virtual card
// enrollment attempt.
const base::FeatureParam<int>
    kAutofillVirtualCardEnrollDelayInStrikeDatabaseInDays{
        &kAutofillEnforceDelaysInStrikeDatabase,
        "autofill_virtual_card_enroll_delay_in_strike_database_in_days", 7};

bool ShouldShowImprovedUserConsentForCreditCardSave() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // The new user consent UI is fully launched on MacOS, Windows and Linux.
  return true;
#else
  // Chrome OS does not have the new UI.
  return false;
#endif
}

}  // namespace autofill::features
