// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::features {

// When enabled, the BNPL flow acts as if the user has not yet seen the AI
// terms. This allows the AI terms to be shown as bold font repeatedly for
// testing purposes, regardless of the actual stored user preference.
BASE_FEATURE(kAutofillAiBasedAmountExtractionIgnoreSeenTermsForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, users are given the option to use their phone camera to scan
// their credit card when adding it via Autofill iOS settings.
BASE_FEATURE(kAutofillCreditCardScannerIos, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables testing BNPL in countries where it would otherwise be disabled. This
// is a testing flag that should never be enabled.
BASE_FEATURE(kAutofillDisableBnplCountryCheckForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will extract the checkout amount from the checkout
// page using server-side AI.
BASE_FEATURE(kAutofillEnableAiBasedAmountExtraction,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will extract the checkout amount from the checkout page
// of the allowlisted merchant websites.
BASE_FEATURE(kAutofillEnableAmountExtraction,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Enables testing of the result of checkout amount extraction on desktop.
// This flag will allow amount extraction to run on any website when a CC
// form is clicked. This flag should never be enabled.
BASE_FEATURE(kAutofillEnableAmountExtractionTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, users are given the bottom sheet suggestion to scan credit
// card, and save and fill the card information.
BASE_FEATURE(kAutofillEnableBottomSheetScanCardAndFill,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, buy now pay later (BNPL) in Autofill will be offered.
BASE_FEATURE(kAutofillEnableBuyNowPayLater,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// When enabled, additional steps are required to autofill buy now pay later
// (BNPL) issuers that are externally linked.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterForExternallyLinked,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, buy now pay later (BNPL) for Klarna in Autofill will be
// offered.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterForKlarna,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, buy now pay later (BNPL) data will be synced to Chrome clients.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterSyncing,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, the second line string in a BNPL suggestion is updated to
// include the issuer names for better brand recognition.
BASE_FEATURE(kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// When enabled, card benefits offered by American Express will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardBenefitsForAmericanExpress,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, card benefits offered by BMO will be shown in Payments Autofill
// UI.
BASE_FEATURE(kAutofillEnableCardBenefitsForBmo,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

// When enabled, Chrome will show metadata along with other card information
// when the virtual card is presented to users.
BASE_FEATURE(kAutofillEnableCardBenefitsSync,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, runtime retrieval of CVC along with card number and expiry
// from issuer for enrolled cards will be enabled during form fill.
BASE_FEATURE(kAutofillEnableCardInfoRuntimeRetrieval,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, we will store CVC for both local and server credit cards. This
// will also allow the users to autofill their CVCs on checkout pages.
BASE_FEATURE(kAutofillEnableCvcStorageAndFilling,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, will enhance CVV storage project. Provide better suggestion,
// resolve conflict with COF project and add logging.
BASE_FEATURE(kAutofillEnableCvcStorageAndFillingEnhancement,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, this will enhance the CVV storage project. The enhancement will
// enable CVV storage suggestions for standalone CVC fields.
BASE_FEATURE(kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, in-product help UI will be shown the first time a card added
// outside of Chrome appears in Autofill card suggestions."
BASE_FEATURE(kAutofillEnableDownstreamCardAwarenessIph,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card flat rate benefit will not be shown on merchants in the
// blocklist.
BASE_FEATURE(kAutofillEnableFlatRateCardBenefitsBlocklist,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, flat rate card benefits sourced from Curinos will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableFlatRateCardBenefitsFromCurinos,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

// When enabled, server card retrieval will begin with a risk-based check
// instead of jumping straight to CVC or biometric auth.
BASE_FEATURE(kAutofillEnableFpanRiskBasedAuthentication,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, updates the American Express network art in Autofill.
BASE_FEATURE(kAutofillEnableNewAmexNetworkArt,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the card benefits toggle in settings will show updated text.
BASE_FEATURE(kAutofillEnableNewCardBenefitsToggleText,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, card and IBAN autofill will be shown in new FOP style.
BASE_FEATURE(kAutofillEnableNewFopDisplayAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, offers will be displayed in the Clank keyboard accessory during
// downstream.
BASE_FEATURE(kAutofillEnableOffersInClankKeyboardAccessory,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will use the Pay Now Pay Later tabs UI for payments
// autofill when buy now pay later options are available for the merchant
// webpage.
BASE_FEATURE(kAutofillEnablePayNowPayLaterTabs,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// When enabled, in use-cases where we would not have triggered any interactive
// authentication to autofill payment methods, we will trigger a device
// authentication on ChromeOS.
BASE_FEATURE(kAutofillEnablePaymentsMandatoryReauthChromeOs,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// When enabled, risk data is prefetched during payments autofill flows to
// reduce user-perceived latency.
BASE_FEATURE(kAutofillEnablePrefetchingRiskDataForRetrieval,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the 'Save and Fill' suggestion will be offered in the credit
// card dropdown menu for users who don't have any cards saved in Autofill.
BASE_FEATURE(kAutofillEnableSaveAndFill, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, show Pix settings as a separate preference menu item instead of
// bundling them together with the non-card payment preference menu item.
BASE_FEATURE(kAutofillEnableSeparatePixPreferenceItem,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// When enabled, the Touch To Fill bottom sheet on Android can be reshown after
// a BNPL flow is dismissed by a user.
BASE_FEATURE(kAutofillEnableTouchToFillReshowForBnpl,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, Chrome will trigger 3DS authentication during a virtual card
// retrieval if a challenge is required, 3DS authentication is available for
// the card, and FIDO is not.
BASE_FEATURE(kAutofillEnableVcn3dsAuthentication,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// When enabled, the Virtual Card enrollment bottom sheet uses the Java
// payments data manager and associated image fetcher to retrieve the cached
// card art images (when available on Android). When not enabled, the native
// payments data manager and associated image fetcher are used, where the image
// is not cached.
BASE_FEATURE(kAutofillEnableVirtualCardJavaPaymentsDataManager,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, certain strings and logos referencing Google Account, Google
// Payments, and Google Pay will instead reference Google Wallet.
BASE_FEATURE(kAutofillEnableWalletBranding, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Payments Autofill Buy Now Pay Later (BNPL) will use each
// corresponding issuer's blocklist instead of allowlist to check for website
// eligibility.
BASE_FEATURE(kAutofillPreferBuyNowPayLaterBlocklists,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// When enabled, this feature prioritizes showing the save card bubble over the
// mandatory re-auth bubble when both are applicable.
BASE_FEATURE(kAutofillPrioritizeSaveCardOverMandatoryReauth,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome will try to fetch payment account image resources again
// upon failure. The number of attempts is a controllable parameter. This is a
// kill-switch.
// TODO(crbug.com/40276036): Clean up after M139 branch (June 23, 2025).
BASE_FEATURE(kAutofillRetryImageFetchOnFailure,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we will store autofill server card data in shared storage.
BASE_FEATURE(kAutofillSharedStorageServerCardData,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, manual fill view will be shown directly from form focusing
// events, if a virtual card has been retrieved previously.
BASE_FEATURE(kAutofillShowManualFillForVirtualCards,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Kill switch, when enabled, will prevent the display of the save card bubble
// within a tab modal pop-up window.
BASE_FEATURE(kAutofillSkipSaveCardForTabModalPopup,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, eWallet accounts are synced from the Google Payments servers
// and displayed on the payment methods settings page.
BASE_FEATURE(kAutofillSyncEwalletAccounts, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillTouchToFillShowManualFillForVcnFix,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, adds a timeout on the network request for Unmask requests.
BASE_FEATURE(kAutofillUnmaskCardRequestTimeout,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because the feature state depends on the user's country.
// The set of launched countries is listed in autofill_experiments.cc, and this
// flag remains as a way to easily enable upload credit card save for testers,
// as well as enable non-fully-launched countries on a trial basis.
BASE_FEATURE(kAutofillUpstream, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, updates the VCN strike database with different values of
// kExpiryTimeDelta as part of of the VCN strike optimization experiment.
// See go/vcn-strike-optimization-design.
BASE_FEATURE(kAutofillVcnEnrollStrikeExpiryTime,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillVcnEnrollStrikeExpiryTimeDays{
    &kAutofillVcnEnrollStrikeExpiryTime, "autofill_vcn_strike_expiry_time_days",
    /*default_value=*/180};

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
