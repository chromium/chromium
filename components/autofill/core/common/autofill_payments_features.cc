// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::features {

#if BUILDFLAG(IS_IOS)
// When enabled, save card fix flow values for missing cardholder name and
// expiry date won't be defaulted as detected on iOS.
BASE_FEATURE(kAutofillDisableDefaultSaveCardFixFlowDetection,
             "AutofillDisableDefaultSaveCardFixFlowDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, card category benefits offered by BMO will be shown in Autofill
// suggestions on the allowlisted merchant websites.
BASE_FEATURE(kAutofillEnableAllowlistForBmoCardCategoryBenefits,
             "AutofillEnableAllowlistForBmoCardCategoryBenefits",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome will have the ability to load and query the allowlist
// for checkout amount extraction, which will be used to check if the current
// URL is eligible for products that use the checkout amount extraction
// algorithm.
BASE_FEATURE(kAutofillEnableAmountExtractionAllowlistDesktop,
             "AutofillEnableAmountExtractionAllowlistDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome will extract the checkout amount from the checkout page
// of the allowlisted merchant websites.
BASE_FEATURE(kAutofillEnableAmountExtractionDesktop,
             "AutofillEnableAmountExtractionDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables logging of the result of checkout amount extraction on desktop.
// This flag will allow amount extraction to run on any website when a CC
// form is clicked. This flag should never be enabled.
BASE_FEATURE(kAutofillEnableAmountExtractionDesktopLogging,
             "AutofillEnableAmountExtractionDesktopLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, buy now pay later (BNPL) in Autofill will be offered.
BASE_FEATURE(kAutofillEnableBuyNowPayLater,
             "AutofillEnableBuyNowPayLater",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, buy now pay later (BNPL) data will be synced to Chrome clients.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterSyncing,
             "AutofillEnableBuyNowPayLaterSyncing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, card benefits offered by American Express will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardBenefitsForAmericanExpress,
             "AutofillEnableCardBenefitsForAmericanExpress",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, card benefits offered by BMO will be shown in Payments Autofill
// UI.
BASE_FEATURE(kAutofillEnableCardBenefitsForBmo,
             "AutofillEnableCardBenefitsForBmo",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card benefits IPH will be shown in Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardBenefitsIph,
             "AutofillEnableCardBenefitsIph",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card benefit source will be synced to Chrome clients.
BASE_FEATURE(kAutofillEnableCardBenefitsSourceSync,
             "AutofillEnableCardBenefitsSourceSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome will show metadata along with other card information
// when the virtual card is presented to users.
BASE_FEATURE(kAutofillEnableCardBenefitsSync,
             "AutofillEnableCardBenefitsSync",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, runtime retrieval of CVC along with card number and expiry
// from issuer for enrolled cards will be enabled during form fill.
BASE_FEATURE(kAutofillEnableCardInfoRuntimeRetrieval,
             "AutofillEnableCardInfoRuntimeRetrieval",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, we will store CVC for both local and server credit cards. This
// will also allow the users to autofill their CVCs on checkout pages.
BASE_FEATURE(kAutofillEnableCvcStorageAndFilling,
             "AutofillEnableCvcStorageAndFilling",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, will enhance CVV storage project. Provide better suggestion,
// resolve conflict with COF project and add logging.
BASE_FEATURE(kAutofillEnableCvcStorageAndFillingEnhancement,
             "AutofillEnableCvcStorageAndFillingEnhancement",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, this will enhance the CVV storage project. The enhancement will
// enable CVV storage suggestions for standalone CVC fields.
BASE_FEATURE(kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement,
             "AutofillEnableCvcStorageAndFillingStandaloneFormEnhancement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card flat rate benefit will not be shown on merchants in the
// blocklist.
BASE_FEATURE(kAutofillEnableFlatRateCardBenefitsBlocklist,
             "AutofillEnableFlatRateCardBenefitsBlocklist",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, flat rate card benefits sourced from Curinos will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableFlatRateCardBenefitsFromCurinos,
             "AutofillEnableFlatRateCardBenefitsFromCurinos",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, server card retrieval will begin with a risk-based check
// instead of jumping straight to CVC or biometric auth.
BASE_FEATURE(kAutofillEnableFpanRiskBasedAuthentication,
             "AutofillEnableFpanRiskBasedAuthentication",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, the Virtual Card enrollment bottom sheet uses the Java
// payments data manager and associated image fetcher to retrieve the cached
// card art images (when available on Android). When not enabled, the native
// payments data manager and associated image fetcher are used, where the image
// is not cached.
BASE_FEATURE(kAutofillEnableVirtualCardJavaPaymentsDataManager,
             "AutofillEnableVirtualCardJavaPaymentsDataManager",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, a form event will log to all of the parsed forms of the same
// type on a webpage. This means credit card form events will log to all credit
// card form types and address form events will log to all address form types."
// TODO(crbug.com/359934323): Clean up when launched
BASE_FEATURE(kAutofillEnableLogFormEventsToAllParsedFormTypes,
             "AutofillEnableLogFormEventsToAllParsedFormTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, virtual card downstream enrollment will support multiple
// requests at a time.
BASE_FEATURE(kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment,
             "AutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the card benefits toggle in settings will show updated text.
BASE_FEATURE(kAutofillEnableNewCardBenefitsToggleText,
             "AutofillEnableNewCardBenefitsToggleText",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, card and IBAN autofill will be shown in new FOP style.
BASE_FEATURE(kAutofillEnableNewFopDisplayDesktop,
             "AutofillEnableNewFopDisplayDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// When enabled, the 'Save and Fill' suggestion will be offered in the credit
// card dropdown menu for users who don't have any cards saved in Autofill.
BASE_FEATURE(kAutofillEnableSaveAndFill,
             "AutofillEnableSaveAndFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, save card securely message be displayed on upload card
// UI message.
BASE_FEATURE(kAutofillEnableShowSaveCardSecurelyMessage,
             "AutofillEnableShowSaveCardSecurelyMessage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Pix bank accounts are synced from Chrome Sync backend and
// stored in the local db.
BASE_FEATURE(kAutofillEnableSyncingOfPixBankAccounts,
             "AutofillEnableSyncingOfPixBankAccounts",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, Chrome will trigger 3DS authentication during a virtual card
// retrieval if a challenge is required, 3DS authentication is available for
// the card, and FIDO is not.
BASE_FEATURE(kAutofillEnableVcn3dsAuthentication,
             "AutofillEnableVcn3dsAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, save card bottomsheet will be shown to save the card locally
// when the user has not previously rejected the offer to save the card.
BASE_FEATURE(kAutofillLocalSaveCardBottomSheet,
             "AutofillLocalSaveCardBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, Chrome will try to fetch payment account image resources again
// upon failure. The number of attempts is a controllable parameter. This is a
// kill-switch.
// TODO(crbug.com/40276036): Clean up after M139 branch (June 23, 2025).
BASE_FEATURE(kAutofillRetryImageFetchOnFailure,
             "AutofillRetryImageFetchOnFailure",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, save card bottomsheet will be shown to save the card to the
// server when the user has not previously rejected the offer to save the card,
// and both a valid expiry date and cardholder name are present.
BASE_FEATURE(kAutofillSaveCardBottomSheet,
             "AutofillSaveCardBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// If enabled, we will store autofill server card data in shared storage.
BASE_FEATURE(kAutofillSharedStorageServerCardData,
             "AutofillSharedStorageServerCardData",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, manual fill view will be shown directly from form focusing
// events, if a virtual card has been retrieved previously.
BASE_FEATURE(kAutofillShowManualFillForVirtualCards,
             "AutofillShowManualFillForVirtualCards",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Kill switch, when enabled, will prevent the display of the save card bubble
// within a tab modal pop-up window.
BASE_FEATURE(kAutofillSkipSaveCardForTabModalPopup,
             "AutofillSkipSaveCardForTabModalPopup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, adds a timeout on the network request for Unmask requests.
BASE_FEATURE(kAutofillUnmaskCardRequestTimeout,
             "AutofillUnmaskCardRequestTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, adds a timeout on the network request for UploadCard requests.
BASE_FEATURE(kAutofillUploadCardRequestTimeout,
             "AutofillUploadCardRequestTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillUploadCardRequestTimeoutMilliseconds{
    &kAutofillUploadCardRequestTimeout,
    "autofill_upload_card_request_timeout_milliseconds",
    /*default_value=*/6500};

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because the feature state depends on the user's country.
// The set of launched countries is listed in autofill_experiments.cc, and this
// flag remains as a way to easily enable upload credit card save for testers,
// as well as enable non-fully-launched countries on a trial basis.
BASE_FEATURE(kAutofillUpstream,
             "AutofillUpstream",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, adds a timeout on the network request for VcnEnroll requests.
BASE_FEATURE(kAutofillVcnEnrollRequestTimeout,
             "AutofillVcnEnrollRequestTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillVcnEnrollRequestTimeoutMilliseconds{
    &kAutofillVcnEnrollRequestTimeout,
    "autofill_vcn_enroll_request_timeout_milliseconds",
    /*default_value=*/6500};

// When enabled, updates the VCN strike database with different values of
// kExpiryTimeDelta as part of of the VCN strike optimization experiment.
// See go/vcn-strike-optimization-design.
BASE_FEATURE(kAutofillVcnEnrollStrikeExpiryTime,
             "AutofillVcnEnrollStrikeExpiryTime",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillVcnEnrollStrikeExpiryTimeDays{
    &kAutofillVcnEnrollStrikeExpiryTime, "autofill_vcn_strike_expiry_time_days",
    /*default_value=*/180};

#if BUILDFLAG(IS_ANDROID)
// When enabled, eWallet accounts are synced from the Google Payments servers
// and displayed on the payment methods settings page.
BASE_FEATURE(kAutofillSyncEwalletAccounts,
             "AutofillSyncEwalletAccounts",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

bool ShouldShowImprovedUserConsentForCreditCardSave() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
  // The new user consent UI is fully launched on MacOS, Windows and Linux.
  return true;
#else
  // Chrome OS does not have the new UI.
  return false;
#endif
}

}  // namespace autofill::features
