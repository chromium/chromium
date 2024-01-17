// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "url/origin.h"

namespace autofill {

// Exposes some testing operations for CreditCardBenefit shared fields.
class CreditCardBenefitTestApi {
 public:
  explicit CreditCardBenefitTestApi(CreditCardBenefit* benefit)
      : benefit_(*benefit) {}

  void SetBenefitIdForTesting(CreditCardBenefit::BenefitId benefit_id) {
    benefit_->benefit_id_ = std::move(benefit_id);
  }
  void SetLinkedCardInstrumentIdForTesting(
      CreditCardBenefit::LinkedCardInstrumentId linked_card_instrument_id) {
    benefit_->linked_card_instrument_id_ = linked_card_instrument_id;
  }
  void SetBenefitDescriptionForTesting(std::u16string benefit_description) {
    benefit_->benefit_description_ = std::move(benefit_description);
  }
  void SetStartTimeForTesting(base::Time start_time) {
    benefit_->start_time_ = start_time;
  }
  void SetEndTimeForTesting(base::Time expiry_time) {
    benefit_->expiry_time_ = expiry_time;
  }

 private:
  const raw_ref<CreditCardBenefit> benefit_;
};

// Exposes some testing operations for CreditCardCategoryBenefit.
class CreditCardCategoryBenefitTestApi : public CreditCardBenefitTestApi {
 public:
  explicit CreditCardCategoryBenefitTestApi(
      CreditCardCategoryBenefit* category_benefit)
      : CreditCardBenefitTestApi(category_benefit),
        category_benefit_(*category_benefit) {}

  void SetBenefitCategoryForTesting(
      CreditCardCategoryBenefit::BenefitCategory benefit_category) {
    category_benefit_->benefit_category_ = benefit_category;
  }

 private:
  const raw_ref<CreditCardCategoryBenefit> category_benefit_;
};

// Exposes some testing operations for CreditCardMerchantBenefit.
class CreditCardMerchantBenefitTestApi : public CreditCardBenefitTestApi {
 public:
  explicit CreditCardMerchantBenefitTestApi(
      CreditCardMerchantBenefit* merchant_benefit)
      : CreditCardBenefitTestApi(merchant_benefit),
        merchant_benefit_(*merchant_benefit) {}

  void SetMerchantDomainsForTesting(
      base::flat_set<url::Origin> merchant_domains) {
    merchant_benefit_->merchant_domains_ = std::move(merchant_domains);
  }

 private:
  const raw_ref<CreditCardMerchantBenefit> merchant_benefit_;
};

inline CreditCardBenefitTestApi test_api(CreditCardBenefit& benefit) {
  return CreditCardBenefitTestApi(&benefit);
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
