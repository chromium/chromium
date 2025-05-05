// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace autofill {

namespace {

optimization_guide::proto::OptimizationType
GetVcnMerchantOptOutOptimizationTypeForCard(const CreditCard& card) {
  // If `card` is not enrolled into VCN, do not return an optimization type.
  if (card.virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::kEnrolled) {
    return optimization_guide::proto::TYPE_UNSPECIFIED;
  }

  // If `card` is not a network-level enrollment, do not return an optimization
  // type.
  if (card.virtual_card_enrollment_type() !=
      CreditCard::VirtualCardEnrollmentType::kNetwork) {
    return optimization_guide::proto::TYPE_UNSPECIFIED;
  }

  // If there is an optimization type present for the card's network, then
  // return that optimization type.
  if (card.network() == kVisaCard) {
    return optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA;
  } else if (card.network() == kMasterCard) {
    return optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD;
  } else if (card.network() == kDiscoverCard) {
    return optimization_guide::proto::VCN_MERCHANT_OPT_OUT_DISCOVER;
  }

  // No conditions to return an optimization type were found, so return that we
  // could not find an optimization type.
  return optimization_guide::proto::TYPE_UNSPECIFIED;
}

std::vector<optimization_guide::proto::OptimizationType>
GetCardBenefitsOptimizationTypesForCard(const CreditCard& card) {
  std::vector<optimization_guide::proto::OptimizationType> optimization_types;
  if (card.issuer_id() == kAmexCardIssuerId) {
    optimization_types.push_back(
        optimization_guide::proto::
            AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::
            AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS);
  } else if (card.issuer_id() == kBmoCardIssuerId &&
             base::FeatureList::IsEnabled(
                 features::
                     kAutofillEnableAllowlistForBmoCardCategoryBenefits)) {
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_AIR_MILES_PARTNER_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_ALCOHOL_STORE_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_DINING_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_DRUGSTORE_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_ENTERTAINMENT_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_GROCERY_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_OFFICE_SUPPLY_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_RECURRING_BILL_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_TRANSIT_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_TRAVEL_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_WHOLESALE_CLUB_BENEFITS);
  } else if (card.issuer_id() == kCapitalOneCardIssuerId) {
    optimization_types.push_back(
        optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_DINING_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_GROCERY_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::
            CAPITAL_ONE_CREDIT_CARD_ENTERTAINMENT_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_STREAMING_BENEFITS);
    optimization_types.push_back(
        optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_BENEFITS_BLOCKED);
  }
  return optimization_types;
}

void AddCreditCardOptimizationTypes(
    base::span<const CreditCard* const> cards,
    base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  for (const CreditCard* card : cards) {
    auto vcn_merchant_opt_out_optimization_type =
        GetVcnMerchantOptOutOptimizationTypeForCard(*card);
    if (vcn_merchant_opt_out_optimization_type !=
        optimization_guide::proto::TYPE_UNSPECIFIED) {
      optimization_types.insert(vcn_merchant_opt_out_optimization_type);
    }

    // Check if the card is eligible for category-level benefit
    // optimizations from supported issuers. Other benefit types are read
    // directly from the `PaymentsDataManager` and don't require filter
    // optimizations.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableCardBenefitsSync)) {
      auto benefits_optimization_types =
          GetCardBenefitsOptimizationTypesForCard(*card);
      if (!benefits_optimization_types.empty()) {
        optimization_types.insert(benefits_optimization_types.begin(),
                                  benefits_optimization_types.end());
      }
    }
  }
}

void AddAblationOptimizationTypes(
    base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  optimization_types.insert(
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1);
  optimization_types.insert(
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST2);
  optimization_types.insert(
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST3);
  optimization_types.insert(
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST4);
  optimization_types.insert(
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST5);
  optimization_types.insert(
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST6);
}

// Maps the credit card category optimizations type to the
// CreditCardCategoryBenefit::BenefitCategory enum.
CreditCardCategoryBenefit::BenefitCategory
GetBenefitCategoryForOptimizationType(
    const optimization_guide::proto::OptimizationType& optimization_type) {
  switch (optimization_type) {
    case optimization_guide::proto::
        AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kFlights;
    case optimization_guide::proto::
        AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kSubscription;
    case optimization_guide::proto::BMO_CREDIT_CARD_AIR_MILES_PARTNER_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kAirMilesPartner;
    case optimization_guide::proto::BMO_CREDIT_CARD_ALCOHOL_STORE_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kAlcoholStores;
    case optimization_guide::proto::BMO_CREDIT_CARD_DRUGSTORE_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kDrugstores;
    case optimization_guide::proto::BMO_CREDIT_CARD_OFFICE_SUPPLY_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kOfficeSupplies;
    case optimization_guide::proto::BMO_CREDIT_CARD_RECURRING_BILL_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kRecurringBills;
    case optimization_guide::proto::BMO_CREDIT_CARD_TRANSIT_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kTransit;
    case optimization_guide::proto::BMO_CREDIT_CARD_TRAVEL_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kTravel;
    case optimization_guide::proto::BMO_CREDIT_CARD_WHOLESALE_CLUB_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kWholesaleClubs;
    case optimization_guide::proto::BMO_CREDIT_CARD_DINING_BENEFITS:
    case optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_DINING_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kDining;
    case optimization_guide::proto::BMO_CREDIT_CARD_GROCERY_BENEFITS:
    case optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_GROCERY_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kGroceryStores;
    case optimization_guide::proto::BMO_CREDIT_CARD_ENTERTAINMENT_BENEFITS:
    case optimization_guide::proto::
        CAPITAL_ONE_CREDIT_CARD_ENTERTAINMENT_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kEntertainment;
    case optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_STREAMING_BENEFITS:
      return CreditCardCategoryBenefit::BenefitCategory::kStreaming;
    default:
      NOTREACHED();
  }
}

}  // namespace

AutofillOptimizationGuide::AutofillOptimizationGuide(
    optimization_guide::OptimizationGuideDecider* decider)
    : decider_(decider) {}

AutofillOptimizationGuide::~AutofillOptimizationGuide() = default;

void AutofillOptimizationGuide::OnDidParseForm(
    const FormStructure& form_structure,
    const PaymentsDataManager& payments_data_manager) {
  // This flat set represents all of the optimization types that we need to
  // register based on `form_structure`.
  base::flat_set<optimization_guide::proto::OptimizationType>
      optimization_types;

  const bool has_iban_field =
      std::ranges::any_of(form_structure, [](const auto& field) {
        return field->Type().GetStorableType() == IBAN_VALUE;
      });
  if (has_iban_field) {
    optimization_types.insert(optimization_guide::proto::IBAN_AUTOFILL_BLOCKED);
  }
  const bool has_credit_card_field =
      std::ranges::any_of(form_structure, [](const auto& field) {
        return field->Type().group() == FieldTypeGroup::kCreditCard;
      });

  std::vector<const CreditCard*> server_cards;
  if (has_credit_card_field) {
    server_cards = payments_data_manager.GetServerCreditCards();
    AddCreditCardOptimizationTypes(server_cards, optimization_types);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  auto bnpl_issuer_allowlist_can_be_loaded =
      [&payments_data_manager](BnplIssuer::IssuerId issuer_id) {
        return base::Contains(payments_data_manager.GetBnplIssuers(), issuer_id,
                              &BnplIssuer::issuer_id) &&
               base::FeatureList::IsEnabled(
                   features::kAutofillEnableAmountExtractionAllowlistDesktop);
      };

  if (bnpl_issuer_allowlist_can_be_loaded(BnplIssuer::IssuerId::kBnplAffirm)) {
    optimization_types.insert(
        optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM);
  }

  if (bnpl_issuer_allowlist_can_be_loaded(BnplIssuer::IssuerId::kBnplZip)) {
    optimization_types.insert(
        optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP);
  }
#endif

  if (base::FeatureList::IsEnabled(features::kAutofillEnableAblationStudy)) {
    AddAblationOptimizationTypes(optimization_types);
  }

  // If we do not have any optimization types to register, do not do anything.
  if (!optimization_types.empty()) {
    // Register all optimization types that we need based on `form_structure`.
    decider_->RegisterOptimizationTypes(
        std::vector<optimization_guide::proto::OptimizationType>(
            std::move(optimization_types).extract()));
  }
}

CreditCardCategoryBenefit::BenefitCategory
AutofillOptimizationGuide::AttemptToGetEligibleCreditCardBenefitCategory(
    std::string_view issuer_id,
    const GURL& url) const {
  std::vector<optimization_guide::proto::OptimizationType>
      issuer_optimization_types;
  if (issuer_id == kAmexCardIssuerId) {
    issuer_optimization_types.push_back(
        optimization_guide::proto::
            AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::
            AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS);
  } else if (issuer_id == kBmoCardIssuerId &&
             base::FeatureList::IsEnabled(
                 features::
                     kAutofillEnableAllowlistForBmoCardCategoryBenefits)) {
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_AIR_MILES_PARTNER_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_ALCOHOL_STORE_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_DINING_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_DRUGSTORE_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_ENTERTAINMENT_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_GROCERY_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_OFFICE_SUPPLY_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_RECURRING_BILL_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_TRANSIT_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_TRAVEL_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::BMO_CREDIT_CARD_WHOLESALE_CLUB_BENEFITS);
  } else if (issuer_id == kCapitalOneCardIssuerId) {
    issuer_optimization_types.push_back(
        optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_DINING_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_GROCERY_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::
            CAPITAL_ONE_CREDIT_CARD_ENTERTAINMENT_BENEFITS);
    issuer_optimization_types.push_back(
        optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_STREAMING_BENEFITS);
  }

  for (auto& optimization_type : issuer_optimization_types) {
    optimization_guide::OptimizationGuideDecision decision =
        decider_->CanApplyOptimization(url, optimization_type,
                                       /*optimization_metadata=*/nullptr);
    if (decision == optimization_guide::OptimizationGuideDecision::kTrue) {
      // Webpage is eligible for category benefit `optimization_type`. Early
      // return is fine as a website can only fall under one category for an
      // issuer.
      return GetBenefitCategoryForOptimizationType(optimization_type);
    }
  }
  // No applicable category benefit for the 'issuer_id' on the 'url'.
  return CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory;
}

bool AutofillOptimizationGuide::ShouldBlockSingleFieldSuggestions(
    const GURL& url,
    const AutofillField* field) const {
  // If the field's storable type is `IBAN_VALUE`, check whether IBAN
  // suggestions should be blocked based on `url`.
  if (field->Type().GetStorableType() == IBAN_VALUE) {
    optimization_guide::OptimizationGuideDecision decision =
        decider_->CanApplyOptimization(
            url, optimization_guide::proto::IBAN_AUTOFILL_BLOCKED,
            /*optimization_metadata=*/nullptr);
    // Since the optimization guide decider integration corresponding to
    // `IBAN_AUTOFILL_BLOCKED` lists are blocklists for the question "Can this
    // site be optimized?" a match on the blocklist answers the question with
    // "no". Therefore, ...::kFalse indicates that `url` is blocked from
    // displaying IBAN suggestions. If the optimization type was not
    // registered in time when we queried it, it will be `kUnknown`, so the
    // default functionality in this case will be to not block the suggestions
    // from being shown.
    return decision == optimization_guide::OptimizationGuideDecision::kFalse;
  }

  // No conditions indicating single field suggestions should be blocked were
  // encountered, so return that they should not be blocked.
  return false;
}

bool AutofillOptimizationGuide::ShouldBlockFormFieldSuggestion(
    const GURL& url,
    const CreditCard& card) const {
  if (auto optimization_type =
          GetVcnMerchantOptOutOptimizationTypeForCard(card);
      optimization_type != optimization_guide::proto::TYPE_UNSPECIFIED) {
    optimization_guide::OptimizationGuideDecision decision =
        decider_->CanApplyOptimization(url, optimization_type,
                                       /*optimization_metadata=*/nullptr);
    // Since the optimization guide decider integration corresponding to VCN
    // merchant opt-out lists are blocklists for the question "Can this site
    // be optimized?" a match on the blocklist answers the question with "no".
    // Therefore, ...::kFalse indicates that `url` is blocked from displaying
    // this suggestion. If the optimization type was not registered
    // in time when we queried it, it will be `kUnknown`, so the default
    // functionality in this case will be to not block the suggestion from
    // being shown.
    return decision == optimization_guide::OptimizationGuideDecision::kFalse;
  }

  // No conditions to block displaying this virtual card suggestion were met,
  // so return that we should not block displaying this suggestion.
  return false;
}

bool AutofillOptimizationGuide::IsEligibleForAblation(
    const GURL& url,
    optimization_guide::proto::OptimizationType type) const {
  CHECK(type == optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1 ||
        type == optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST2 ||
        type == optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST3 ||
        type == optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST4 ||
        type == optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST5 ||
        type == optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST6)
      << type;
  optimization_guide::OptimizationGuideDecision decision =
      decider_->CanApplyOptimization(url, type,
                                     /*optimization_metadata=*/nullptr);
  return decision == optimization_guide::OptimizationGuideDecision::kTrue;
}

bool AutofillOptimizationGuide::ShouldBlockBenefitSuggestionLabelsForCardAndUrl(
    const CreditCard& card,
    const GURL& url) const {
  if (card.issuer_id() == kCapitalOneCardIssuerId) {
    optimization_guide::OptimizationGuideDecision decision =
        decider_->CanApplyOptimization(
            url,
            optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_BENEFITS_BLOCKED,
            /*optimization_metadata=*/nullptr);
    // Since the Capital One benefit suggestions hint uses a blocklist, it will
    // return kFalse if the `url` is present, meaning when kFalse is returned,
    // we should block the suggestion from being shown. If the optimization type
    // was not registered in time before being queried, it will be kUnknown, so
    // the default functionality in this case will be to not block the
    // suggestion from being shown.
    return decision == optimization_guide::OptimizationGuideDecision::kFalse;
  }

  // No conditions indicating benefits suggestions should be blocked were
  // encountered, so return that they should not be blocked.
  return false;
}

bool AutofillOptimizationGuide::IsUrlEligibleForBnplIssuer(
    BnplIssuer::IssuerId issuer_id,
    const GURL& url) const {
  auto can_apply_optimization = [&url, this](
                                    optimization_guide::proto::OptimizationType
                                        issuer_optimization_type) {
    return decider_->CanApplyOptimization(url, issuer_optimization_type,
                                          /*optimization_metadata=*/nullptr) ==
           optimization_guide::OptimizationGuideDecision::kTrue;
  };
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAmountExtractionAllowlistDesktop)) {
    switch (issuer_id) {
      case BnplIssuer::IssuerId::kBnplAffirm:
        return can_apply_optimization(
            optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM);
      case BnplIssuer::IssuerId::kBnplZip:
        return can_apply_optimization(
            optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP);
      // TODO(crbug.com/408268581): Handle Afterpay issuer enum value when
      // adding Afterpay to the BNPL flow.
      case BnplIssuer::IssuerId::kBnplAfterpay:
        NOTREACHED();
    }
    NOTREACHED();
  }
  return false;
}

}  // namespace autofill
