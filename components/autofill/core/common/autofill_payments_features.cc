// Copyright 2019 The Chromium Authors
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
BASE_FEATURE(kAutofillAlwaysReturnCloudTokenizedCard,
             "AutofillAlwaysReturnCloudTokenizedCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, manual fallback will be auto-triggered on form interaction in
// the case where autofill failed to fill a credit card form accurately.
BASE_FEATURE(kAutofillAutoTriggerManualFallbackForCards,
             "AutofillAutoTriggerManualFallbackForCards",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card art images (instead of network icons) will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardArtImage,
             "AutofillEnableCardArtImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card product name (instead of issuer network) will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardProductName,
             "AutofillEnableCardProductName",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, if the user encounters the yellow path (challenge path) in the
// VCN retrieval flow and the server denotes that the card is eligible for CVC
// authentication, CVC authentication will be offered as one of the challenge
// options.
BASE_FEATURE(kAutofillEnableCvcForVcnYellowPath,
             "AutofillEnableCvcForVcnYellowPath",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, user's will see network card art images and network icons which
// are larger, having a white border, and don't have the standard grey overlay
// applied to them.
BASE_FEATURE(kAutofillEnableNewCardArtAndNetworkImages,
             "AutofillEnableNewCardArtAndNetworkImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a progress dialog will display while authenticating with FIDO.
// TODO(crbug.com/1337380): Clean up kAutofillEnableFIDOProgressDialog when it's
// fully rolled out.
BASE_FEATURE(kAutofillEnableFIDOProgressDialog,
             "AutofillEnableFIDOProgressDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, client side URL filtering will be triggered for the IBAN
// use-case, so that IBAN autofill is not offered on websites that are blocked.
BASE_FEATURE(kAutofillEnableIbanClientSideUrlFiltering,
             "AutofillEnableIbanClientSideUrlFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, enable manual falling component for virtual cards on Android.
BASE_FEATURE(kAutofillEnableManualFallbackForVirtualCards,
             "AutofillEnableManualFallbackForVirtualCards",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, client side URL filtering will be triggered for the merchant
// opt-out use-case, so that virtual card suggestions are not shown on websites
// that are opted-out of virtual cards.
BASE_FEATURE(kAutofillEnableMerchantOptOutClientSideUrlFiltering,
             "AutofillEnableMerchantOptOutClientSideUrlFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the user will see a new banner logo and text in the bubble
// offering to Upstream their cards onto Google Pay.
BASE_FEATURE(kAutofillEnableNewSaveCardBubbleUi,
             "AutofillEnableNewSaveCardBubbleUi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a notification will be displayed on page navigation if the
// domain has an eligible merchant promo code offer or reward.
BASE_FEATURE(kAutofillEnableOfferNotificationForPromoCodes,
             "AutofillEnableOfferNotificationForPromoCodes",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, offers will be displayed in the Clank keyboard accessory during
// downstream.
BASE_FEATURE(kAutofillEnableOffersInClankKeyboardAccessory,
             "AutofillEnableOffersInClankKeyboardAccessory",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, some extra metrics logging for Autofill Downstream will start.
BASE_FEATURE(kAutofillEnableRemadeDownstreamMetrics,
             "AutofillEnableRemadeDownstreamMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, if the user interacts with the manual fallback bottom sheet
// on Android, it'll remain sticky until the user dismisses it.
BASE_FEATURE(kAutofillEnableStickyManualFallbackForCards,
             "AutofillEnableStickyManualFallbackForCards",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the user will have the ability to update the virtual card
// enrollment of a credit card through their chrome browser after certain
// autofill flows (for example, downstream and upstream), and from the settings
// page.
BASE_FEATURE(kAutofillEnableUpdateVirtualCardEnrollment,
             "AutofillEnableUpdateVirtualCardEnrollment",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// When enabled, the option of using cloud token virtual card will be offered
// when all requirements are met.
BASE_FEATURE(kAutofillEnableVirtualCard,
             "AutofillEnableVirtualCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, after a successful authentication to autofill a virtual card,
// the user will be prompted to opt-in to FIDO if the user is not currently
// opted-in, and if the user is opted-in already and the virtual card is FIDO
// eligible the user will be prompted to register the virtual card into FIDO.
BASE_FEATURE(kAutofillEnableVirtualCardFidoEnrollment,
             "AutofillEnableVirtualCardFidoEnrollment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, in the payments settings page on desktop, virtual card
// enrollment management will be provided so that the user can enroll/unenroll a
// card in virtual card.
BASE_FEATURE(kAutofillEnableVirtualCardManagementInDesktopSettingsPage,
             "AutofillEnableVirtualCardManagementInDesktopSettingsPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome will show metadata along with other card information
// when the virtual card is presented to users.
BASE_FEATURE(kAutofillEnableVirtualCardMetadata,
             "AutofillEnableVirtualCardMetadata",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, if the previous feature offer was declined, a delay will be
// added before Chrome attempts to show offer again.
BASE_FEATURE(kAutofillEnforceDelaysInStrikeDatabase,
             "AutofillEnforceDelaysInStrikeDatabase",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Autofill will attempt to fill IBAN (International Bank Account
// Number) fields when data is available.
BASE_FEATURE(kAutofillFillIbanFields,
             "AutofillFillIbanFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will attempt to fill merchant promo/coupon/gift code
// fields when data is available.
BASE_FEATURE(kAutofillFillMerchantPromoCodeFields,
             "AutofillFillMerchantPromoCodeFields",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Autofill will offer saving a card to the users when the Chrome
// detects a card number with the last 4 digits that matches an existing server
// card but has a different expiration date.
BASE_FEATURE(kAutofillOfferToSaveCardWithSameLastFour,
             "AutofillOfferToSaveCardWithSameLastFour",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will attempt to find International Bank Account Number
// (IBAN) fields when parsing forms.
BASE_FEATURE(kAutofillParseIBANFields,
             "AutofillParseIBANFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will attempt to find standalone CVC fields for VCN
// card on file when parsing forms.
BASE_FEATURE(kAutofillParseVcnCardOnFileStandaloneCvcFields,
             "AutofillParseVcnCardOnFileStandaloneCvcFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Expiration and Type titles will be removed from Chrome
// payment settings page.
BASE_FEATURE(kAutofillRemoveCardExpirationAndTypeTitles,
             "AutofillRemoveCardExpirationAndTypeTitles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the Save Card infobar supports editing before submitting.
BASE_FEATURE(kAutofillSaveCardInfobarEditSupport,
             "AutofillSaveCardInfobarEditSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the entire PAN and the CVC details of the unmasked cached card
// will be shown in the manual filling view.
BASE_FEATURE(kAutofillShowUnmaskedCachedCardInManualFillingView,
             "AutofillShowUnmaskedCachedCardInManualFillingView",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill suggestions that consist of a local and server
// version of the same card will attempt to fill the server card upon selection
// instead of the local card.
BASE_FEATURE(kAutofillSuggestServerCardInsteadOfLocalCard,
             "AutofillSuggestServerCardInsteadOfLocalCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because the feature state depends on the user's country.
// The set of launched countries is listed in autofill_experiments.cc, and this
// flag remains as a way to easily enable upload credit card save for testers,
// as well as enable non-fully-launched countries on a trial basis.
BASE_FEATURE(kAutofillUpstream,
             "AutofillUpstream",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome allows credit card upload to Google Payments if the
// user's email domain is from a common email provider (thus unlikely to be an
// enterprise or education user).
BASE_FEATURE(kAutofillUpstreamAllowAdditionalEmailDomains,
             "AutofillUpstreamAllowAdditionalEmailDomains",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome allows credit card upload to Google Payments, no matter
// the user's email domain.
BASE_FEATURE(kAutofillUpstreamAllowAllEmailDomains,
             "AutofillUpstreamAllowAllEmailDomains",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, sets the OAuth2 token in GetUploadDetails requests to Google
// Payments, in order to provide a better experience for users with server-side
// features disabled but not client-side features.
BASE_FEATURE(kAutofillUpstreamAuthenticatePreflightCall,
             "AutofillUpstreamAuthenticatePreflightCall",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the secure data type for cards sent during credit card upload
// save is updated to match newer server requirements.
BASE_FEATURE(kAutofillUpstreamUseAlternateSecureDataType,
             "AutofillUpstreamUseAlternateSecureDataType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, we use the Elo regex to match the BIN ranges.
BASE_FEATURE(kAutofillUseEloRegexForBinMatching,
             "AutofillUseEloRegexForBinMatching",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
