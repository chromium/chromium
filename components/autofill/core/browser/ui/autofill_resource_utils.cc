// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_resource_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/grit/components_scaled_resources.h"

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
  return true;
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
    {Suggestion::Icon::kBnplGeneric, IDR_AUTOFILL_METADATA_BNPL_GENERIC_OLD},
    {Suggestion::Icon::kBnplAffirm, IDR_AUTOFILL_METADATA_AFFIRM},
    {Suggestion::Icon::kBnplAfterpay, IDR_AUTOFILL_METADATA_AFTERPAY},
    {Suggestion::Icon::kBnplKlarna, IDR_AUTOFILL_METADATA_KLARNA},
    {Suggestion::Icon::kBnplZip, IDR_AUTOFILL_METADATA_ZIP},
#if BUILDFLAG(IS_ANDROID)
    {Suggestion::Icon::kHome, IDR_ANDROID_AUTOFILL_HOME},
    {Suggestion::Icon::kScanCreditCard, IDR_ANDROID_AUTOFILL_CC_SCAN_NEW},
    {Suggestion::Icon::kOfferTag, IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN},
    {Suggestion::Icon::kWork, IDR_ANDROID_AUTOFILL_WORK},
    {Suggestion::Icon::kAndroidMessages, IDR_ANDROID_AUTOFILL_ANDROID_MESSAGES},
    {Suggestion::Icon::kRecoveryPassword, IDR_ANDROID_PASSWORD_HISTORY},
    {Suggestion::Icon::kCardGenericSpark,
     IDR_ANDROID_AUTOFILL_CARD_GENERIC_SPARK},
    {Suggestion::Icon::kCardGenericVector,
     IDR_ANDROID_AUTOFILL_CARD_GENERIC_VECTOR},
    {Suggestion::Icon::kIdCard, IDR_ANDROID_AUTOFILL_ID_CARD},
    {Suggestion::Icon::kIdCard2, IDR_ANDROID_AUTOFILL_ID_CARD_2},
    {Suggestion::Icon::kIdCard2Spark, IDR_ANDROID_AUTOFILL_ID_CARD_2_SPARK},
    {Suggestion::Icon::kIdCardSpark, IDR_ANDROID_AUTOFILL_ID_CARD_SPARK},
    {Suggestion::Icon::kFlight, IDR_ANDROID_AUTOFILL_FLIGHT},
    {Suggestion::Icon::kFlightSpark, IDR_ANDROID_AUTOFILL_FLIGHT_SPARK},
    {Suggestion::Icon::kLocation, IDR_ANDROID_AUTOFILL_LOCATION},
    {Suggestion::Icon::kLocationSpark, IDR_ANDROID_AUTOFILL_LOCATION_SPARK},
    {Suggestion::Icon::kOrder, IDR_ANDROID_AUTOFILL_SHOPPING_BAG},
    {Suggestion::Icon::kOrderSpark, IDR_ANDROID_AUTOFILL_SHOPPING_BAG_SPARK},
    {Suggestion::Icon::kPersonCheck, IDR_ANDROID_AUTOFILL_PERSON_CHECK},
    {Suggestion::Icon::kShipment, IDR_ANDROID_AUTOFILL_SHIPMENT},
    {Suggestion::Icon::kShipmentSpark, IDR_ANDROID_AUTOFILL_SHIPMENT_SPARK},
    {Suggestion::Icon::kVehicle, IDR_ANDROID_AUTOFILL_VEHICLE},
    {Suggestion::Icon::kVehicleSpark, IDR_ANDROID_AUTOFILL_CAR_SPARK},
    {Suggestion::Icon::kPassport, IDR_ANDROID_AUTOFILL_PASSPORT},
    {Suggestion::Icon::kPassportSpark, IDR_ANDROID_AUTOFILL_PASSPORT_SPARK},
    {Suggestion::Icon::kSpark, IDR_ANDROID_AUTOFILL_SPARK},
    {Suggestion::Icon::kTextSpark, IDR_ANDROID_AUTOFILL_TEXT_SPARK},
    {Suggestion::Icon::kEmail, IDR_ANDROID_AUTOFILL_EMAIL},
    {Suggestion::Icon::kSadTab, IDR_ANDROID_AUTOFILL_SAD_TAB},
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
    {Suggestion::Icon::kBnplGeneric, IDR_AUTOFILL_METADATA_BNPL_GENERIC},
    {Suggestion::Icon::kBnplAffirm, IDR_AUTOFILL_METADATA_AFFIRM},
    {Suggestion::Icon::kBnplAfterpay, IDR_AUTOFILL_METADATA_AFTERPAY},
    {Suggestion::Icon::kBnplKlarna, IDR_AUTOFILL_METADATA_KLARNA},
    {Suggestion::Icon::kBnplZip, IDR_AUTOFILL_METADATA_ZIP},
#if BUILDFLAG(IS_ANDROID)
    {Suggestion::Icon::kHome, IDR_ANDROID_AUTOFILL_HOME},
    {Suggestion::Icon::kScanCreditCard, IDR_ANDROID_AUTOFILL_CC_SCAN_NEW},
    {Suggestion::Icon::kOfferTag, IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN},
    {Suggestion::Icon::kWork, IDR_ANDROID_AUTOFILL_WORK},
    {Suggestion::Icon::kAndroidMessages, IDR_ANDROID_AUTOFILL_ANDROID_MESSAGES},
    {Suggestion::Icon::kRecoveryPassword, IDR_ANDROID_PASSWORD_HISTORY},
    {Suggestion::Icon::kCardGenericSpark,
     IDR_ANDROID_AUTOFILL_CARD_GENERIC_SPARK},
    {Suggestion::Icon::kCardGenericVector,
     IDR_ANDROID_AUTOFILL_CARD_GENERIC_VECTOR},
    {Suggestion::Icon::kIdCard, IDR_ANDROID_AUTOFILL_ID_CARD},
    {Suggestion::Icon::kIdCard2, IDR_ANDROID_AUTOFILL_ID_CARD_2},
    {Suggestion::Icon::kIdCard2Spark, IDR_ANDROID_AUTOFILL_ID_CARD_2_SPARK},
    {Suggestion::Icon::kIdCardSpark, IDR_ANDROID_AUTOFILL_ID_CARD_SPARK},
    {Suggestion::Icon::kFlight, IDR_ANDROID_AUTOFILL_FLIGHT},
    {Suggestion::Icon::kFlightSpark, IDR_ANDROID_AUTOFILL_FLIGHT_SPARK},
    {Suggestion::Icon::kLocation, IDR_ANDROID_AUTOFILL_LOCATION},
    {Suggestion::Icon::kLocationSpark, IDR_ANDROID_AUTOFILL_LOCATION_SPARK},
    {Suggestion::Icon::kOrder, IDR_ANDROID_AUTOFILL_SHOPPING_BAG},
    {Suggestion::Icon::kOrderSpark, IDR_ANDROID_AUTOFILL_SHOPPING_BAG_SPARK},
    {Suggestion::Icon::kPersonCheck, IDR_ANDROID_AUTOFILL_PERSON_CHECK},
    {Suggestion::Icon::kShipment, IDR_ANDROID_AUTOFILL_SHIPMENT},
    {Suggestion::Icon::kShipmentSpark, IDR_ANDROID_AUTOFILL_SHIPMENT_SPARK},
    {Suggestion::Icon::kVehicle, IDR_ANDROID_AUTOFILL_VEHICLE},
    {Suggestion::Icon::kVehicleSpark, IDR_ANDROID_AUTOFILL_CAR_SPARK},
    {Suggestion::Icon::kPassport, IDR_ANDROID_AUTOFILL_PASSPORT},
    {Suggestion::Icon::kPassportSpark, IDR_ANDROID_AUTOFILL_PASSPORT_SPARK},
    {Suggestion::Icon::kSpark, IDR_ANDROID_AUTOFILL_SPARK},
    {Suggestion::Icon::kTextSpark, IDR_ANDROID_AUTOFILL_TEXT_SPARK},
    {Suggestion::Icon::kEmail, IDR_ANDROID_AUTOFILL_EMAIL},
    {Suggestion::Icon::kSadTab, IDR_ANDROID_AUTOFILL_SAD_TAB},
#endif  // BUILDFLAG(IS_ANDROID)
});

}  // namespace

int GetIconResourceID(Suggestion::Icon resource_name) {
  if ((resource_name == Suggestion::Icon::kCardAmericanExpress) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableNewAmexNetworkArt)) {
    return IDR_AUTOFILL_METADATA_CC_AMEX_NEW;
  }

  if (ShouldUseNewFopDisplay()) {
    auto it = kDataResources.find(resource_name);
    return it == kDataResources.end() ? kResourceNotFoundId : it->second;
  }
  auto it = kOldDataResources.find(resource_name);
  return it == kOldDataResources.end() ? kResourceNotFoundId : it->second;
}

}  // namespace autofill
