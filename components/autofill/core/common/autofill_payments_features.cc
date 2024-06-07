// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

#include "build/chromeos_buildflags.h"

namespace autofill::features {

// When enabled, Android N+ devices will be supported for FIDO authentication.
BASE_FEATURE(kAutofillEnableAndroidNKeyForFidoAuthentication,
             "AutofillEnableAndroidNKeyForFidoAuthentication",
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

// When enabled, card benefits offered by issuers will be synced from the
// Payments server.
BASE_FEATURE(kAutofillEnableCardBenefitsSync,
             "AutofillEnableCardBenefitsSync",
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

// When enabled, user's will see network card art images and network icons which
// are larger, having a white border, and don't have the standard grey overlay
// applied to them.
BASE_FEATURE(kAutofillEnableNewCardArtAndNetworkImages,
             "AutofillEnableNewCardArtAndNetworkImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a progress dialog will display while authenticating with FIDO.
// TODO(crbug.com/40229268): Clean up kAutofillEnableFIDOProgressDialog when
// it's fully rolled out.
BASE_FEATURE(kAutofillEnableFIDOProgressDialog,
             "AutofillEnableFIDOProgressDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// When enabled, the merchant_domain field is included in requests to unmask a
// card.
BASE_FEATURE(kAutofillEnableMerchantDomainInUnmaskCardRequest,
             "AutofillEnableMerchantDomainInUnmaskCardRequest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the GPay logo will be moved to the right side in payments
// autofill dialogs and bubbles on desktop.
BASE_FEATURE(kAutofillEnableMovingGPayLogoToTheRightOnDesktop,
             "AutofillEnableMovingGPayLogoToTheRightOnDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// When enabled, risk data is prefetched during payments autofill flows to reduce
// user-perceived latency.
BASE_FEATURE(kAutofillEnablePrefetchingRiskDataForRetrieval,
             "AutofillEnablePrefetchingRiskDataForRetrieval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, some extra metrics logging for Autofill Downstream will start.
BASE_FEATURE(kAutofillEnableRemadeDownstreamMetrics,
             "AutofillEnableRemadeDownstreamMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the save card screen will present a loading spinner when
// uploading the card to the server and present a confirmation screen with the
// result when completed.
BASE_FEATURE(kAutofillEnableSaveCardLoadingAndConfirmation,
             "AutofillEnableSaveCardLoadingAndConfirmation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, save card will fallback to a local save if the server upload of
// a card encounters a failure.
BASE_FEATURE(kAutofillEnableSaveCardLocalSaveFallback,
             "AutofillEnableSaveCardLocalSaveFallback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will attempt to offer upload save for IBANs
// (International Bank Account Numbers) and autofill server-based IBANs.
BASE_FEATURE(kAutofillEnableServerIban,
             "AutofillEnableServerIban",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, if the user interacts with the manual fallback bottom sheet
// on Android, it'll remain sticky until the user dismisses it.
BASE_FEATURE(kAutofillEnableStickyManualFallbackForCards,
             "AutofillEnableStickyManualFallbackForCards",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will display grayed out virtual card suggestions on
// merchant websites where the merchant has opted-out of virtual cards.
BASE_FEATURE(kAutofillEnableVcnGrayOutForMerchantOptOut,
             "AutofillEnableVcnGrayOutForMerchantOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, uses the refactored VirtualCardEnrollMetricsLogger in
// VirtualCardEnrollBubbleController on all platforms.
BASE_FEATURE(kAutofillEnableVirtualCardEnrollMetricsLogger,
             "AutofillEnableVirtualCardEnrollMetricsLogger",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Verve-branded card art will be shown for Verve cards.
BASE_FEATURE(kAutofillEnableVerveCardSupport,
             "AutofillEnableVerveCardSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

#if BUILDFLAG(IS_ANDROID)
// When enabled, IBAN Autofill suggestions are shown via the keyboard accessory
// instead of the bottom sheet.
BASE_FEATURE(kAutofillSkipAndroidBottomSheetForIban,
             "AutofillSkipAndroidBottomSheetForIban",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

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

#if BUILDFLAG(IS_IOS)
// When enabled, use two '•' when displaying the last four digits of a credit
// card number. (E.g., '•• 8888' rather than '•••• 8888').
BASE_FEATURE(kAutofillUseTwoDotsForLastFourDigits,
             "AutofillUseTwoDotsForLastFourDigits",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When this is enabled, virtual card enrollment and retrieval will be enabled
// on Bling.
BASE_FEATURE(kAutofillEnableVirtualCards,
             "AutofillEnableVirtualCards",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

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
