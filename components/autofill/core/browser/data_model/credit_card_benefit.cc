// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card_benefit.h"

#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

CreditCardBenefitBase::CreditCardBenefitBase(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    std::u16string benefit_description,
    base::Time start_time,
    base::Time expiry_time)
    : benefit_id_(std::move(benefit_id)),
      linked_card_instrument_id_(linked_card_instrument_id),
      benefit_description_(std::move(benefit_description)),
      start_time_(start_time),
      expiry_time_(expiry_time) {}

CreditCardBenefitBase::CreditCardBenefitBase(const CreditCardBenefitBase&) =
    default;
CreditCardBenefitBase& CreditCardBenefitBase::operator=(
    const CreditCardBenefitBase&) = default;
CreditCardBenefitBase::CreditCardBenefitBase(CreditCardBenefitBase&&) = default;
CreditCardBenefitBase& CreditCardBenefitBase::operator=(
    CreditCardBenefitBase&&) = default;
CreditCardBenefitBase::~CreditCardBenefitBase() = default;

bool CreditCardBenefitBase::IsActiveBenefit() const {
  base::Time current_time = AutofillClock::Now();
  return current_time >= start_time_ && current_time < expiry_time_;
}

bool CreditCardBenefitBase::IsValidForWriteFromSync() const {
  return linked_card_instrument_id_ && !benefit_id_->empty() &&
         !benefit_description_.empty() && AutofillClock::Now() < expiry_time_;
}

CreditCardFlatRateBenefit::CreditCardFlatRateBenefit(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    std::u16string benefit_description,
    base::Time start_time,
    base::Time expiry_time)
    : CreditCardBenefitBase(benefit_id,
                            linked_card_instrument_id,
                            benefit_description,
                            start_time,
                            expiry_time) {}

CreditCardFlatRateBenefit::CreditCardFlatRateBenefit(
    const CreditCardFlatRateBenefit&) = default;
CreditCardFlatRateBenefit& CreditCardFlatRateBenefit::operator=(
    const CreditCardFlatRateBenefit&) = default;
CreditCardFlatRateBenefit::CreditCardFlatRateBenefit(
    CreditCardFlatRateBenefit&&) = default;
CreditCardFlatRateBenefit& CreditCardFlatRateBenefit::operator=(
    CreditCardFlatRateBenefit&&) = default;
CreditCardFlatRateBenefit::~CreditCardFlatRateBenefit() = default;

bool CreditCardFlatRateBenefit::IsValidForWriteFromSync() const {
  return CreditCardBenefitBase::IsValidForWriteFromSync();
}

CreditCardCategoryBenefit::CreditCardCategoryBenefit(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    BenefitCategory benefit_category,
    std::u16string benefit_description,
    base::Time start_time,
    base::Time expiry_time)
    : CreditCardBenefitBase(benefit_id,
                            linked_card_instrument_id,
                            benefit_description,
                            start_time,
                            expiry_time),
      benefit_category_(benefit_category) {}

CreditCardCategoryBenefit::CreditCardCategoryBenefit(
    const CreditCardCategoryBenefit&) = default;
CreditCardCategoryBenefit& CreditCardCategoryBenefit::operator=(
    const CreditCardCategoryBenefit&) = default;
CreditCardCategoryBenefit::CreditCardCategoryBenefit(
    CreditCardCategoryBenefit&&) = default;
CreditCardCategoryBenefit& CreditCardCategoryBenefit::operator=(
    CreditCardCategoryBenefit&&) = default;
CreditCardCategoryBenefit::~CreditCardCategoryBenefit() = default;

bool CreditCardCategoryBenefit::IsValidForWriteFromSync() const {
  return CreditCardBenefitBase::IsValidForWriteFromSync() &&
         benefit_category_ != BenefitCategory::kUnknownBenefitCategory;
}

CreditCardMerchantBenefit::CreditCardMerchantBenefit(
    BenefitId benefit_id,
    LinkedCardInstrumentId linked_card_instrument_id,
    std::u16string benefit_description,
    base::flat_set<url::Origin> merchant_domains,
    base::Time start_time,
    base::Time expiry_time)
    : CreditCardBenefitBase(benefit_id,
                            linked_card_instrument_id,
                            benefit_description,
                            start_time,
                            expiry_time),
      merchant_domains_(std::move(merchant_domains)) {}

CreditCardMerchantBenefit::CreditCardMerchantBenefit(
    const CreditCardMerchantBenefit&) = default;
CreditCardMerchantBenefit& CreditCardMerchantBenefit::operator=(
    const CreditCardMerchantBenefit&) = default;
CreditCardMerchantBenefit::CreditCardMerchantBenefit(
    CreditCardMerchantBenefit&&) = default;
CreditCardMerchantBenefit& CreditCardMerchantBenefit::operator=(
    CreditCardMerchantBenefit&&) = default;
CreditCardMerchantBenefit::~CreditCardMerchantBenefit() = default;

bool CreditCardMerchantBenefit::IsValidForWriteFromSync() const {
  return CreditCardBenefitBase::IsValidForWriteFromSync() &&
         !merchant_domains_.empty();
}

}  // namespace autofill
