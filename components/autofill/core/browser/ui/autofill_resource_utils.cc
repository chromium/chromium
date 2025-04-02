// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_resource_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
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

bool ShouldUseNewFopDisplay() {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  return false;
#else
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableNewFopDisplayDesktop);
#endif
}

constexpr auto kOldDataResources = base::MakeFixedFlatMap<Suggestion::Icon,
                                                          int>({
    {Suggestion::Icon::kCardAmericanExpress, IDR_AUTOFILL_METADATA_CC_AMEX_OLD},
    {Suggestion::Icon::kCardDiners, IDR_AUTOFILL_METADATA_CC_DINERS_OLD},
    {Suggestion::Icon::kCardDiscover, IDR_AUTOFILL_METADATA_CC_DISCOVER_OLD},
    {Suggestion::Icon::kCardElo, IDR_AUTOFILL_METADATA_CC_ELO_OLD},
    {Suggestion::Icon::kCardGeneric, IDR_AUTOFILL_METADATA_CC_GENERIC_OLD},
    {Suggestion::Icon::kCardJCB, IDR_AUTOFILL_METADATA_CC_JCB_OLD},
    {Suggestion::Icon::kCardMasterCard,
     IDR_AUTOFILL_METADATA_CC_MASTERCARD_OLD},
    {Suggestion::Icon::kCardMir, IDR_AUTOFILL_METADATA_CC_MIR_OLD},
    {Suggestion::Icon::kCardTroy, IDR_AUTOFILL_METADATA_CC_TROY_OLD},
    {Suggestion::Icon::kCardUnionPay, IDR_AUTOFILL_METADATA_CC_UNIONPAY_OLD},
    {Suggestion::Icon::kCardVerve, IDR_AUTOFILL_METADATA_CC_VERVE_OLD},
    {Suggestion::Icon::kCardVisa, IDR_AUTOFILL_METADATA_CC_VISA_OLD},
    {Suggestion::Icon::kIban, IDR_AUTOFILL_IBAN_OLD},
    {Suggestion::Icon::kBnpl, IDR_AUTOFILL_METADATA_BNPL_GENERIC_OLD},
#if BUILDFLAG(IS_ANDROID)
    {Suggestion::Icon::kHome, IDR_ANDROID_AUTOFILL_HOME},
    {Suggestion::Icon::kHttpWarning, IDR_ANDROID_AUTOFILL_HTTP_WARNING},
    {Suggestion::Icon::kHttpsInvalid,
     IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING},
    {Suggestion::Icon::kScanCreditCard, IDR_ANDROID_AUTOFILL_CC_SCAN_NEW},
    {Suggestion::Icon::kSettingsAndroid, IDR_ANDROID_AUTOFILL_SETTINGS},
    {Suggestion::Icon::kCreate, IDR_ANDROID_AUTOFILL_CREATE},
    {Suggestion::Icon::kOfferTag, IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN},
    {Suggestion::Icon::kPlusAddress, IDR_AUTOFILL_PLUS_ADDRESS},
    {Suggestion::Icon::kWork, IDR_ANDROID_AUTOFILL_WORK},
#endif  // BUILDFLAG(IS_ANDROID)
});

constexpr auto kDataResources = base::MakeFixedFlatMap<Suggestion::Icon, int>({
    {Suggestion::Icon::kCardAmericanExpress, IDR_AUTOFILL_METADATA_CC_AMEX},
    {Suggestion::Icon::kCardDiners, IDR_AUTOFILL_METADATA_CC_DINERS},
    {Suggestion::Icon::kCardDiscover, IDR_AUTOFILL_METADATA_CC_DISCOVER},
    {Suggestion::Icon::kCardElo, IDR_AUTOFILL_METADATA_CC_ELO},
    {Suggestion::Icon::kCardGeneric, IDR_AUTOFILL_METADATA_CC_GENERIC},
    {Suggestion::Icon::kCardJCB, IDR_AUTOFILL_METADATA_CC_JCB},
    {Suggestion::Icon::kCardMasterCard, IDR_AUTOFILL_METADATA_CC_MASTERCARD},
    {Suggestion::Icon::kCardMir, IDR_AUTOFILL_METADATA_CC_MIR},
    {Suggestion::Icon::kCardTroy, IDR_AUTOFILL_METADATA_CC_TROY},
    {Suggestion::Icon::kCardUnionPay, IDR_AUTOFILL_METADATA_CC_UNIONPAY},
    {Suggestion::Icon::kCardVerve, IDR_AUTOFILL_METADATA_CC_VERVE},
    {Suggestion::Icon::kCardVisa, IDR_AUTOFILL_METADATA_CC_VISA},
    {Suggestion::Icon::kIban, IDR_AUTOFILL_IBAN},
    {Suggestion::Icon::kBnpl, IDR_AUTOFILL_METADATA_BNPL_GENERIC},
#if BUILDFLAG(IS_ANDROID)
    {Suggestion::Icon::kHome, IDR_ANDROID_AUTOFILL_HOME},
    {Suggestion::Icon::kHttpWarning, IDR_ANDROID_AUTOFILL_HTTP_WARNING},
    {Suggestion::Icon::kHttpsInvalid,
     IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING},
    {Suggestion::Icon::kScanCreditCard, IDR_ANDROID_AUTOFILL_CC_SCAN_NEW},
    {Suggestion::Icon::kSettingsAndroid, IDR_ANDROID_AUTOFILL_SETTINGS},
    {Suggestion::Icon::kCreate, IDR_ANDROID_AUTOFILL_CREATE},
    {Suggestion::Icon::kOfferTag, IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN},
    {Suggestion::Icon::kPlusAddress, IDR_AUTOFILL_PLUS_ADDRESS},
    {Suggestion::Icon::kWork, IDR_ANDROID_AUTOFILL_WORK},
#endif  // BUILDFLAG(IS_ANDROID)
});

}  // namespace

int GetIconResourceID(Suggestion::Icon resource_name) {
  if (ShouldUseNewFopDisplay()) {
    auto it = kDataResources.find(resource_name);
    return it == kDataResources.end() ? kResourceNotFoundId : it->second;
  }
  auto it = kOldDataResources.find(resource_name);
  return it == kOldDataResources.end() ? kResourceNotFoundId : it->second;
}

}  // namespace autofill
