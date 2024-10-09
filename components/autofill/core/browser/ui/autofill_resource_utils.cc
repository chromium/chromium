// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_resource_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/resources/android/theme_resources.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

namespace {

// Used in the IDS_ space as a placeholder for resources that don't exist.
constexpr int kResourceNotFoundId = 0;

constexpr auto kDataResources = base::MakeFixedFlatMap<Suggestion::Icon, int>({
    {Suggestion::Icon::kCardAmericanExpress, IDR_AUTOFILL_CC_AMEX},
    {Suggestion::Icon::kCardDiners, IDR_AUTOFILL_CC_DINERS},
    {Suggestion::Icon::kCardDiscover, IDR_AUTOFILL_CC_DISCOVER},
    {Suggestion::Icon::kCardElo, IDR_AUTOFILL_CC_ELO},
    {Suggestion::Icon::kCardGeneric, IDR_AUTOFILL_CC_GENERIC},
    {Suggestion::Icon::kCardJCB, IDR_AUTOFILL_CC_JCB},
    {Suggestion::Icon::kCardMasterCard, IDR_AUTOFILL_CC_MASTERCARD},
    {Suggestion::Icon::kCardMir, IDR_AUTOFILL_CC_MIR},
    {Suggestion::Icon::kCardTroy, IDR_AUTOFILL_CC_TROY},
    {Suggestion::Icon::kCardUnionPay, IDR_AUTOFILL_CC_UNIONPAY},
    {Suggestion::Icon::kCardVerve, IDR_AUTOFILL_CC_VERVE},
    {Suggestion::Icon::kCardVisa, IDR_AUTOFILL_CC_VISA},
    {Suggestion::Icon::kIban, IDR_AUTOFILL_IBAN},
#if BUILDFLAG(IS_ANDROID)
    {Suggestion::Icon::kHttpWarning, IDR_ANDROID_AUTOFILL_HTTP_WARNING},
    {Suggestion::Icon::kHttpsInvalid,
     IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING},
    {Suggestion::Icon::kScanCreditCard, IDR_ANDROID_AUTOFILL_CC_SCAN_NEW},
    {Suggestion::Icon::kSettingsAndroid, IDR_ANDROID_AUTOFILL_SETTINGS},
    {Suggestion::Icon::kCreate, IDR_ANDROID_AUTOFILL_CREATE},
    {Suggestion::Icon::kOfferTag, IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN},
    {Suggestion::Icon::kPlusAddress, IDR_AUTOFILL_PLUS_ADDRESS},
#else
    {Suggestion::Icon::kAutofillPredictionImprovements,
     IDR_AUTOFILL_PREDICTION_IMPROVEMENTS_LOGO},
    {Suggestion::Icon::kAutofillPredictionImprovementsDark,
     IDR_AUTOFILL_PREDICTION_IMPROVEMENTS_LOGO_DARK},
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {Suggestion::Icon::kGooglePay, IDR_AUTOFILL_GOOGLE_PAY},
#if !BUILDFLAG(IS_ANDROID)
    {Suggestion::Icon::kGooglePayDark, IDR_AUTOFILL_GOOGLE_PAY_DARK},
#endif  // !BUILDFLAG(IS_ANDROID)
#else
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
});

constexpr auto kNewCardArtAndNetworkDataResources =
    base::MakeFixedFlatMap<Suggestion::Icon, int>({
        {Suggestion::Icon::kCardAmericanExpress, IDR_AUTOFILL_METADATA_CC_AMEX},
        {Suggestion::Icon::kCardDiners, IDR_AUTOFILL_METADATA_CC_DINERS},
        {Suggestion::Icon::kCardDiscover, IDR_AUTOFILL_METADATA_CC_DISCOVER},
        {Suggestion::Icon::kCardElo, IDR_AUTOFILL_METADATA_CC_ELO},
        {Suggestion::Icon::kCardGeneric, IDR_AUTOFILL_METADATA_CC_GENERIC},
        {Suggestion::Icon::kCardJCB, IDR_AUTOFILL_METADATA_CC_JCB},
        {Suggestion::Icon::kCardMasterCard,
         IDR_AUTOFILL_METADATA_CC_MASTERCARD},
        {Suggestion::Icon::kCardMir, IDR_AUTOFILL_METADATA_CC_MIR},
        {Suggestion::Icon::kCardTroy, IDR_AUTOFILL_METADATA_CC_TROY},
        {Suggestion::Icon::kCardUnionPay, IDR_AUTOFILL_METADATA_CC_UNIONPAY},
        {Suggestion::Icon::kCardVerve, IDR_AUTOFILL_METADATA_CC_VERVE},
        {Suggestion::Icon::kCardVisa, IDR_AUTOFILL_METADATA_CC_VISA},
        {Suggestion::Icon::kIban, IDR_AUTOFILL_IBAN},
#if BUILDFLAG(IS_ANDROID)
        {Suggestion::Icon::kHttpWarning, IDR_ANDROID_AUTOFILL_HTTP_WARNING},
        {Suggestion::Icon::kHttpsInvalid,
         IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING},
        {Suggestion::Icon::kScanCreditCard, IDR_ANDROID_AUTOFILL_CC_SCAN_NEW},
        {Suggestion::Icon::kSettingsAndroid, IDR_ANDROID_AUTOFILL_SETTINGS},
        {Suggestion::Icon::kCreate, IDR_ANDROID_AUTOFILL_CREATE},
        {Suggestion::Icon::kOfferTag, IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN},
        {Suggestion::Icon::kPlusAddress, IDR_AUTOFILL_PLUS_ADDRESS},
#else
        {Suggestion::Icon::kAutofillPredictionImprovements,
         IDR_AUTOFILL_PREDICTION_IMPROVEMENTS_LOGO},
        {Suggestion::Icon::kAutofillPredictionImprovementsDark,
         IDR_AUTOFILL_PREDICTION_IMPROVEMENTS_LOGO_DARK},
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        {Suggestion::Icon::kGooglePay, IDR_AUTOFILL_GOOGLE_PAY},
#if !BUILDFLAG(IS_ANDROID)
        {Suggestion::Icon::kGooglePayDark, IDR_AUTOFILL_GOOGLE_PAY_DARK},
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    });

}  // namespace

int GetIconResourceID(Suggestion::Icon resource_name) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (resource_name == Suggestion::Icon::kGooglePay ||
      resource_name == Suggestion::Icon::kGooglePayDark) {
    return 0;
  }
#endif
  const auto& kDataResource =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableNewCardArtAndNetworkImages)
          ? kNewCardArtAndNetworkDataResources
          : kDataResources;
  auto it = kDataResource.find(resource_name);
  return it == kDataResource.end() ? kResourceNotFoundId : it->second;
}

}  // namespace autofill
