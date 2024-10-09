// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

#include "build/chromeos_buildflags.h"

namespace autofill::features {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
// When enabled, Chrome will extract the checkout amount from the checkout page
// of the allowlisted merchant websites.
BASE_FEATURE(kAutofillEnableAmountExtractionDesktop,
             "AutofillEnableAmountExtractionDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, Android N+ devices will be supported for FIDO authentication.
BASE_FEATURE(kAutofillEnableAndroidNKeyForFidoAuthentication,
             "AutofillEnableAndroidNKeyForFidoAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, buy now pay later (BNPL) in Autofill will be offered through
// Affirm.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterForAffirm,
             "AutofillEnableBuyNowPayLaterForAffirm",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, buy now pay later (BNPL) in Autofill will be offered through
// Zip.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterForZip,
             "AutofillEnableBuyNowPayLaterForZip",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, buy now pay later (BNPL) data will be synced to Chrome clients.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterSyncing,
             "AutofillEnableBuyNowPayLaterSyncing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card art images (instead of network icons) will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardArtImage,
             "AutofillEnableCardArtImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, server will return card art images of the exact required
// dimension.
BASE_FEATURE(kAutofillEnableCardArtServerSideStretching,
             "AutofillEnableCardArtServerSideStretching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card benefits offered by American Express will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardBenefitsForAmericanExpress,
             "AutofillEnableCardBenefitsForAmericanExpress",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card benefits offered by Capital One will be shown in Payments
// Autofill UI.
BASE_FEATURE(kAutofillEnableCardBenefitsForCapitalOne,
             "AutofillEnableCardBenefitsForCapitalOne",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card benefits IPH will be shown in Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardBenefitsIph,
             "AutofillEnableCardBenefitsIph",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will show metadata along with other card information
// when the virtual card is presented to users.
BASE_FEATURE(kAutofillEnableCardBenefitsSync,
             "AutofillEnableCardBenefitsSync",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, runtime retrieval of CVC along with card number and expiry
// from issuer for enrolled cards will be enabled during form fill.
BASE_FEATURE(kAutofillEnableCardInfoRuntimeRetrieval,
             "AutofillEnableCardInfoRuntimeRetrieval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card product name (instead of issuer network) will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardProductName,
             "AutofillEnableCardProductName",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, we will store CVC for both local and server credit cards. This
// will also allow the users to autofill their CVCs on checkout pages.
BASE_FEATURE(kAutofillEnableCvcStorageAndFilling,
             "AutofillEnableCvcStorageAndFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, will enhance CVV storage project. Provide better suggestion,
// resolve conflict with COF project and add logging.
BASE_FEATURE(kAutofillEnableCvcStorageAndFillingEnhancement,
             "AutofillEnableCvcStorageAndFillingEnhancement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, this will enhance the CVV storage project. The enhancement will
// enable CVV storage suggestions for standalone CVC fields.
BASE_FEATURE(kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement,
             "AutofillEnableCvcStorageAndFillingStandaloneFormEnhancement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, user's will see network card art images and network icons which
// are larger, having a white border, and don't have the standard grey overlay
// applied to them.
BASE_FEATURE(kAutofillEnableNewCardArtAndNetworkImages,
             "AutofillEnableNewCardArtAndNetworkImages",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, server card retrieval will begin with a risk-based check
// instead of jumping straight to CVC or biometric auth.
BASE_FEATURE(kAutofillEnableFpanRiskBasedAuthentication,
             "AutofillEnableFpanRiskBasedAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, saving and autofilling local IBANs (International Bank Account
// Numbers) will be offered.
BASE_FEATURE(kAutofillEnableLocalIban,
             "AutofillEnableLocalIban",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, a form event will log to all of the parsed forms of the same
// type on a webpage. This means credit card form events will log to all credit
// card form types and address form events will log to all address form types."
// TODO(crbug.com/359934323): Clean up when launched
BASE_FEATURE(kAutofillEnableLogFormEventsToAllParsedFormTypes,
             "AutofillEnableLogFormEventsToAllParsedFormTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the GPay logo will be moved to the right side in payments
// autofill dialogs and bubbles on clank.
BASE_FEATURE(kAutofillEnableMovingGPayLogoToTheRightOnClank,
             "AutofillEnableMovingGPayLogoToTheRightOnClank",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, offers will be displayed in the Clank keyboard accessory during
// downstream.
BASE_FEATURE(kAutofillEnableOffersInClankKeyboardAccessory,
             "AutofillEnableOffersInClankKeyboardAccessory",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, the payment settings page will show a card promo and allow for
// card scans.
BASE_FEATURE(kAutofillEnablePaymentSettingsCardPromoAndScanCard,
             "AutofillEnablePaymentSettingsCardPromoAndScanCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the payment settings page will save new cards to the payment
// server instead of locally.
BASE_FEATURE(kAutofillEnablePaymentSettingsServerCardSave,
             "AutofillEnablePaymentSettingsServerCardSave",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, risk data is prefetched during payments autofill flows to
// reduce user-perceived latency.
BASE_FEATURE(kAutofillEnablePrefetchingRiskDataForRetrieval,
             "AutofillEnablePrefetchingRiskDataForRetrieval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the save card screen will present a loading spinner when
// uploading the card to the server and present a confirmation screen with the
// result when completed.
BASE_FEATURE(kAutofillEnableSaveCardLoadingAndConfirmation,
             "AutofillEnableSaveCardLoadingAndConfirmation",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, save card will fallback to a local save if the server upload of
// a card encounters a failure.
BASE_FEATURE(kAutofillEnableSaveCardLocalSaveFallback,
             "AutofillEnableSaveCardLocalSaveFallback",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, Autofill will attempt to offer upload save for IBANs
// (International Bank Account Numbers) and autofill server-based IBANs.
BASE_FEATURE(kAutofillEnableServerIban,
             "AutofillEnableServerIban",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, Pix bank accounts are synced from Chrome Sync backend and
// stored in the local db.
BASE_FEATURE(kAutofillEnableSyncingOfPixBankAccounts,
             "AutofillEnableSyncingOfPixBankAccounts",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, Chrome will trigger 3DS authentication during a virtual card
// retrieval if a challenge is required, 3DS authentication is available for
// the card, and FIDO is not.
BASE_FEATURE(kAutofillEnableVcn3dsAuthentication,
             "AutofillEnableVcn3dsAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the vcn enroll screen will present a loading spinner while
// enrolling the card to the server and present a confirmation screen with the
// result when completed.
BASE_FEATURE(kAutofillEnableVcnEnrollLoadingAndConfirmation,
             "AutofillEnableVcnEnrollLoadingAndConfirmation",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, Chrome will display grayed out virtual card suggestions on
// merchant websites where the merchant has opted-out of virtual cards.
BASE_FEATURE(kAutofillEnableVcnGrayOutForMerchantOptOut,
             "AutofillEnableVcnGrayOutForMerchantOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Verve-branded card art will be shown for Verve cards.
BASE_FEATURE(kAutofillEnableVerveCardSupport,
             "AutofillEnableVerveCardSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome will show metadata along with other card information
// when the virtual card is presented to users.
BASE_FEATURE(kAutofillEnableVirtualCardMetadata,
             "AutofillEnableVirtualCardMetadata",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, Autofill will attempt to find standalone CVC fields for VCN
// card on file when parsing forms.
BASE_FEATURE(kAutofillParseVcnCardOnFileStandaloneCvcFields,
             "AutofillParseVcnCardOnFileStandaloneCvcFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the "Show cards from your Google Account" Autofill suggestion
// will not be displayed, and Autofill will work as if it had been selected.
BASE_FEATURE(kAutofillRemovePaymentsButterDropdown,
             "AutofillRemovePaymentsButterDropdown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the add/edit local card dialog in chrome://settings on Desktop
// requires that the card number be valid before saving is allowed.
BASE_FEATURE(kAutofillRequireValidLocalCardsInSettings,
             "AutofillRequireValidLocalCardsInSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, manual fill view will be shown directly from form focusing
// events, if a virtual card has been retrieved previously.
BASE_FEATURE(kAutofillShowManualFillForVirtualCards,
             "AutofillShowManualFillForVirtualCards",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// When enabled, IBAN Autofill suggestions are shown via the keyboard accessory
// instead of the bottom sheet.
BASE_FEATURE(kAutofillSkipAndroidBottomSheetForIban,
             "AutofillSkipAndroidBottomSheetForIban",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, adds a timeout on the network request for Unmask requests.
BASE_FEATURE(kAutofillUnmaskCardRequestTimeout,
             "AutofillUnmaskCardRequestTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, adds a timeout on the network request for UploadCard requests.
BASE_FEATURE(kAutofillUploadCardRequestTimeout,
             "AutofillUploadCardRequestTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillUploadCardRequestTimeoutMilliseconds{
    &kAutofillUploadCardRequestTimeout,
    "autofill_upload_card_request_timeout_milliseconds",
    /*default_value=*/7000};

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because the feature state depends on the user's country.
// The set of launched countries is listed in autofill_experiments.cc, and this
// flag remains as a way to easily enable upload credit card save for testers,
// as well as enable non-fully-launched countries on a trial basis.
BASE_FEATURE(kAutofillUpstream,
             "AutofillUpstream",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, shows different text and images in the UI of the credit card
// upload save bubble.
BASE_FEATURE(kAutofillUpstreamUpdatedUi,
             "AutofillUpstreamUpdatedUi",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillUpstreamUpdatedUiTreatment{
    &kAutofillUpstreamUpdatedUi, "autofill_upstream_updated_ui_treatment", 0};

// When enabled, adds a timeout on the network request for VcnEnroll requests.
BASE_FEATURE(kAutofillVcnEnrollRequestTimeout,
             "AutofillVcnEnrollRequestTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillVcnEnrollRequestTimeoutMilliseconds{
    &kAutofillVcnEnrollRequestTimeout,
    "autofill_vcn_enroll_request_timeout_milliseconds",
    /*default_value=*/7500};

#if BUILDFLAG(IS_ANDROID)
// When enabled, eWallet accounts are synced from the Google Payments servers
// and displayed on the payment methods settings page.
BASE_FEATURE(kAutofillSyncEwalletAccounts,
             "AutofillSyncEwalletAccounts",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

bool ShouldShowImprovedUserConsentForCreditCardSave() {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
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
