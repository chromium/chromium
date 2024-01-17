// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "base/types/strong_alias.h"
#include "url/origin.h"

namespace autofill {

// An object that stores card benefit information, i.e., a credit-card-linked
// benefit that users receive when making an online purchase.
class CreditCardBenefit {
 public:
  using BenefitId = base::StrongAlias<class BenefitIdTag, std::string>;
  using LinkedCardInstrumentId =
      ::base::IdType64<class LinkedCardInstrumentIdMarker>;

  // Represents the type of benefit for the credit card.
  enum class BenefitType {
    // Flat rate benefit which applies to all online purchases.
    // Example: 2% cashback on any purchase.
    kFlatRateBenefit,
    // Category benefit which only applies to websites in a specific category.
    // See `CreditCardCategoryBenefit::BenefitCategory` for applicable
    // categories.
    // Example: 5% cashback on travel.
    kCategoryBenefit,
    // Merchant benefit which only applies to specific merchant websites.
    // Example: 5% cashback on Walmart.com.
    kMerchantBenefit,
  };

  CreditCardBenefit(const CreditCardBenefit&) = delete;
  CreditCardBenefit& operator=(const CreditCardBenefit&) = delete;
  virtual ~CreditCardBenefit();

  virtual bool operator==(const CreditCardBenefit& other_benefit) const;
  virtual bool operator!=(const CreditCardBenefit& other_benefit) const;

  virtual bool IsValid() const;

  const BenefitId& benefit_id() const { return benefit_id_; }
  LinkedCardInstrumentId linked_card_instrument_id() const {
    return linked_card_instrument_id_;
  }
  BenefitType benefit_type() const { return benefit_type_; }
  const std::u16string& benefit_description() const {
    return benefit_description_;
  }
  base::Time start_time() const { return start_time_; }
  base::Time expiry_time() const { return expiry_time_; }

 protected:
  friend class CreditCardBenefitTestApi;

  // Constructor to assign values to member fields shared by all benefit types.
  CreditCardBenefit(BenefitId benefit_id,
                    LinkedCardInstrumentId linked_card_instrument_id,
                    BenefitType benefit_type,
                    std::u16string benefit_description,
                    base::Time start_time,
                    base::Time expiry_time);

  // Represents the unique identifier for this benefit. Generated in sync
  // server.
  BenefitId benefit_id_;

  // Represents the unique identifier for the credit card linked to this
  // benefit.
  LinkedCardInstrumentId linked_card_instrument_id_;

  // Represents the type of benefit for the credit card. Flat rate, category,
  // or merchant.
  BenefitType benefit_type_;

  // The benefit description to be shown in the Autofill UI.
  std::u16string benefit_description_;

  // When the benefit is first active and should be displayed.
  base::Time start_time_ = base::Time::Min();

  // When the benefit is no longer active and should no longer be displayed.
  base::Time expiry_time_ = base::Time::Max();
};

// Credit-card-linked benefit that is available to users on any online
// purchases.
class CreditCardFlatRateBenefit : public CreditCardBenefit {
 public:
  CreditCardFlatRateBenefit(BenefitId benefit_id,
                            LinkedCardInstrumentId linked_card_instrument_id,
                            std::u16string benefit_description,
                            base::Time start_time,
                            base::Time expiry_time);
  ~CreditCardFlatRateBenefit() override;

  bool operator==(const CreditCardBenefit& other) const override;
  bool operator!=(const CreditCardBenefit& other) const override;

  // CreditCardBenefit:
  bool IsValid() const override;
};

// Credit-card-linked benefit that users receive when making an online purchases
// in specific shopping categories. For example, a benefit the user will receive
// for purchasing a subscription service online with the linked card.
class CreditCardCategoryBenefit : public CreditCardBenefit {
 public:
  // Represents the category of purchases that the benefit can be applied to.
  // The category numbering should match
  // `google3/moneta/integrator/common/instrument/instrument_offer.proto`.
  enum class BenefitCategory {
    kUnknownBenefitCategory = 0,
    kSubscription = 1,
    kFlights = 2,
    kDining = 3,
    kEntertainment = 4,
    kStreaming = 5,
    kGroceryStores = 6,
    kMaxValue = kGroceryStores,
  };

  CreditCardCategoryBenefit(BenefitId benefit_id,
                            LinkedCardInstrumentId linked_card_instrument_id,
                            BenefitCategory benefit_category,
                            std::u16string benefit_description,
                            base::Time start_time,
                            base::Time expiry_time);
  ~CreditCardCategoryBenefit() override;

  bool operator==(const CreditCardBenefit& other) const override;
  bool operator!=(const CreditCardBenefit& other) const override;

  // CreditCardBenefit:
  bool IsValid() const override;

  BenefitCategory benefit_category() const { return benefit_category_; }

 private:
  friend class CreditCardCategoryBenefitTestApi;

  // Represents the category of purchases that the benefit can be applied to.
  BenefitCategory benefit_category_ = BenefitCategory::kUnknownBenefitCategory;
};

// Credit-card-linked benefit that users receive when purchasing from specific
// merchant websites.
class CreditCardMerchantBenefit : public CreditCardBenefit {
 public:
  CreditCardMerchantBenefit(BenefitId benefit_id,
                            LinkedCardInstrumentId linked_card_instrument_id,
                            std::u16string benefit_description,
                            base::flat_set<url::Origin> merchant_domains,
                            base::Time start_time,
                            base::Time expiry_time);
  ~CreditCardMerchantBenefit() override;

  bool operator==(const CreditCardBenefit& other) const override;
  bool operator!=(const CreditCardBenefit& other) const override;

  // CreditCardBenefit:
  bool IsValid() const override;

  const base::flat_set<url::Origin>& merchant_domains() const {
    return merchant_domains_;
  }

 private:
  friend class CreditCardMerchantBenefitTestApi;

  // The merchant domains that the benefit is eligible on. Expecting one
  // element in the list in general cases.
  // Example: https://www.acme.com
  base::flat_set<url::Origin> merchant_domains_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_BENEFIT_H_
