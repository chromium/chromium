// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "url/origin.h"

namespace autofill {

// Exposes some testing operations for CreditCardBenefitBase shared fields.
class CreditCardBenefitBaseTestApi {
 public:
  explicit CreditCardBenefitBaseTestApi(CreditCardBenefitBase* benefit)
      : benefit_(*benefit) {}

  void SetBenefitId(CreditCardBenefitBase::BenefitId benefit_id) {
    benefit_->benefit_id_ = std::move(benefit_id);
  }
  void SetLinkedCardInstrumentId(
      CreditCardBenefitBase::LinkedCardInstrumentId linked_card_instrument_id) {
    benefit_->linked_card_instrument_id_ = linked_card_instrument_id;
  }
  void SetBenefitDescription(std::u16string benefit_description) {
    benefit_->benefit_description_ = std::move(benefit_description);
  }
  void SetStartTime(base::Time start_time) {
    benefit_->start_time_ = start_time;
  }
  void SetExpiryTime(base::Time expiry_time) {
    benefit_->expiry_time_ = expiry_time;
  }

 private:
  const raw_ref<CreditCardBenefitBase> benefit_;
};

// Exposes some testing operations for CreditCardCategoryBenefit.
class CreditCardCategoryBenefitTestApi : public CreditCardBenefitBaseTestApi {
 public:
  explicit CreditCardCategoryBenefitTestApi(
      CreditCardCategoryBenefit* category_benefit)
      : CreditCardBenefitBaseTestApi(category_benefit),
        category_benefit_(*category_benefit) {}

  void SetBenefitCategory(
      CreditCardCategoryBenefit::BenefitCategory benefit_category) {
    category_benefit_->benefit_category_ = benefit_category;
  }

 private:
  const raw_ref<CreditCardCategoryBenefit> category_benefit_;
};

// Exposes some testing operations for CreditCardMerchantBenefit.
class CreditCardMerchantBenefitTestApi : public CreditCardBenefitBaseTestApi {
 public:
  explicit CreditCardMerchantBenefitTestApi(
      CreditCardMerchantBenefit* merchant_benefit)
      : CreditCardBenefitBaseTestApi(merchant_benefit),
        merchant_benefit_(*merchant_benefit) {}

  void SetMerchantDomains(base::flat_set<url::Origin> merchant_domains) {
    merchant_benefit_->merchant_domains_ = std::move(merchant_domains);
  }

 private:
  const raw_ref<CreditCardMerchantBenefit> merchant_benefit_;
};

inline CreditCardBenefitBaseTestApi test_api(CreditCardBenefit& benefit) {
  return CreditCardBenefitBaseTestApi(absl::visit(
      [](auto& a) -> CreditCardBenefitBase* { return &a; }, benefit));
}

inline CreditCardBenefitBaseTestApi test_api(CreditCardBenefitBase& benefit) {
  return CreditCardBenefitBaseTestApi(&benefit);
}

inline CreditCardCategoryBenefitTestApi test_api(
    CreditCardCategoryBenefit& category_benefit) {
  return CreditCardCategoryBenefitTestApi(&category_benefit);
}

inline CreditCardMerchantBenefitTestApi test_api(
    CreditCardMerchantBenefit& merchant_benefit) {
  return CreditCardMerchantBenefitTestApi(&merchant_benefit);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_TEST_API_H_
