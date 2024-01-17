// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card_benefit.h"

#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

CreditCardBenefit::~CreditCardBenefit() = default;

bool CreditCardBenefit::operator==(
    const CreditCardBenefit& other_benefit) const = default;
bool CreditCardBenefit::operator!=(
    const CreditCardBenefit& other_benefit) const = default;

bool CreditCardBenefit::IsValid() const {
  if (linked_card_instrument_id_.is_null()) {
    return false;
  }

  if ((*benefit_id_).empty()) {
    return false;
  }

  if (benefit_description_.empty()) {
    return false;
  }

  if (AutofillClock::Now() > expiry_time_) {
    return false;
  }

  return true;
}

CreditCardBenefit::CreditCardBenefit(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    BenefitType benefit_type,
    std::u16string benefit_description,
    base::Time start_time,
    base::Time expiry_time)
    : benefit_id_(std::move(benefit_id)),
      linked_card_instrument_id_(linked_card_instrument_id),
      benefit_type_(benefit_type),
      benefit_description_(std::move(benefit_description)),
      start_time_(start_time),
      expiry_time_(expiry_time) {}

CreditCardFlatRateBenefit::CreditCardFlatRateBenefit(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    std::u16string benefit_description,
    base::Time start_time,
    base::Time expiry_time)
    : CreditCardBenefit(benefit_id,
                        linked_card_instrument_id,
                        BenefitType::kFlatRateBenefit,
                        benefit_description,
                        start_time,
                        expiry_time) {}
CreditCardFlatRateBenefit::~CreditCardFlatRateBenefit() = default;

bool CreditCardFlatRateBenefit::operator==(
    const CreditCardBenefit& other_benefit) const {
  return CreditCardBenefit::operator==(other_benefit);
}

bool CreditCardFlatRateBenefit::operator!=(
    const CreditCardBenefit& other_benefit) const {
  return CreditCardBenefit::operator!=(other_benefit);
}

bool CreditCardFlatRateBenefit::IsValid() const {
  return CreditCardBenefit::IsValid() &&
         benefit_type_ == BenefitType::kFlatRateBenefit;
}

CreditCardCategoryBenefit::CreditCardCategoryBenefit(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    BenefitCategory benefit_category,
    std::u16string benefit_description,
    base::Time start_time,
    base::Time expiry_time)
    : CreditCardBenefit(benefit_id,
                        linked_card_instrument_id,
                        BenefitType::kCategoryBenefit,
                        benefit_description,
                        start_time,
                        expiry_time),
      benefit_category_(benefit_category) {}
CreditCardCategoryBenefit::~CreditCardCategoryBenefit() = default;

bool CreditCardCategoryBenefit::operator==(
    const CreditCardBenefit& other_benefit) const {
  if (!CreditCardBenefit::operator==(other_benefit)) {
    return false;
  }

  // Safe to cast since base checks for type.
  const CreditCardCategoryBenefit& converted_other_benefit =
      static_cast<const CreditCardCategoryBenefit&>(other_benefit);
  if (benefit_category_ != converted_other_benefit.benefit_category_) {
    return false;
  }

  return true;
}

bool CreditCardCategoryBenefit::operator!=(
    const CreditCardBenefit& other_benefit) const {
  return !(*this == other_benefit);
}

bool CreditCardCategoryBenefit::IsValid() const {
  return CreditCardBenefit::IsValid() &&
         benefit_type_ == BenefitType::kCategoryBenefit &&
         benefit_category_ != BenefitCategory::kUnknownBenefitCategory;
}

CreditCardMerchantBenefit::CreditCardMerchantBenefit(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    std::u16string benefit_description,
    base::flat_set<url::Origin> merchant_domains,
    base::Time start_time,
    base::Time expiry_time)
    : CreditCardBenefit(benefit_id,
                        linked_card_instrument_id,
                        BenefitType::kMerchantBenefit,
                        benefit_description,
                        start_time,
                        expiry_time),
      merchant_domains_(std::move(merchant_domains)) {}
CreditCardMerchantBenefit::~CreditCardMerchantBenefit() = default;

bool CreditCardMerchantBenefit::operator==(
    const CreditCardBenefit& other_benefit) const {
  if (!CreditCardBenefit::operator==(other_benefit)) {
    return false;
  }

  // Safe to cast since base checks for type.
  const CreditCardMerchantBenefit& converted_other_benefit =
      static_cast<const CreditCardMerchantBenefit&>(other_benefit);
  if (merchant_domains_ != converted_other_benefit.merchant_domains_) {
    return false;
  }

  return true;
}

bool CreditCardMerchantBenefit::operator!=(
    const CreditCardBenefit& other_benefit) const {
  return !(*this == other_benefit);
}

bool CreditCardMerchantBenefit::IsValid() const {
  return CreditCardBenefit::IsValid() &&
         benefit_type_ == BenefitType::kMerchantBenefit &&
         !merchant_domains_.empty();
}

}  // namespace autofill
