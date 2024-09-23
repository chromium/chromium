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
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace autofill {

class CreditCardFlatRateBenefit;
class CreditCardCategoryBenefit;
class CreditCardMerchantBenefit;

// An object that stores card benefit information, i.e., a credit-card-linked
// benefit that users receive when making an online purchase.
using CreditCardBenefit = absl::variant<CreditCardFlatRateBenefit,
                                        CreditCardCategoryBenefit,
                                        CreditCardMerchantBenefit>;

class CreditCardBenefitBase {
 public:
  using BenefitId = base::StrongAlias<class BenefitIdTag, std::string>;
  using LinkedCardInstrumentId =
      base::IdType64<class LinkedCardInstrumentIdMarker>;

  const BenefitId& benefit_id() const { return benefit_id_; }
  LinkedCardInstrumentId linked_card_instrument_id() const {
    return linked_card_instrument_id_;
  }
  const std::u16string& benefit_description() const {
    return benefit_description_;
  }
  base::Time start_time() const { return start_time_; }
  base::Time expiry_time() const { return expiry_time_; }

  // Returns whether the current time is within the benefit's `start_time` and
  // `end_time`, indicating that the benefit is active.
  bool IsActiveBenefit() const;

 protected:
  friend class CreditCardBenefitBaseTestApi;

  CreditCardBenefitBase(const CreditCardBenefitBase&);
  CreditCardBenefitBase& operator=(const CreditCardBenefitBase&);
  CreditCardBenefitBase(CreditCardBenefitBase&&);
  CreditCardBenefitBase& operator=(CreditCardBenefitBase&&);
  ~CreditCardBenefitBase();

  // Constructor to assign values to member fields shared by all benefit types.
  CreditCardBenefitBase(BenefitId benefit_id,
                        LinkedCardInstrumentId linked_card_instrument_id,
                        std::u16string benefit_description,
                        base::Time start_time,
                        base::Time expiry_time);

  // The following is intentional:
  // - operator<=>() and ==() are defined as member so that it's not publicly
  // accessible.
  // - The friendships are necessary to allow the subclass's operators to call
  //   the base class's protected operators. Limiting friendship to the derived
  //   class's operators does not work nicely until Clang implements
  //   https://reviews.llvm.org/D103929.
  bool operator==(const CreditCardBenefitBase& b) const = default;
  auto operator<=>(const CreditCardBenefitBase& b) const = default;
  friend class CreditCardFlatRateBenefit;
  friend class CreditCardCategoryBenefit;
  friend class CreditCardMerchantBenefit;

  bool IsValidForWriteFromSync() const;

  // Represents the unique identifier for this benefit. Generated in sync
  // server.
  BenefitId benefit_id_;

  // Represents the unique identifier for the credit card linked to this
  // benefit.
  LinkedCardInstrumentId linked_card_instrument_id_;

  // The benefit description to be shown in the Autofill UI.
  std::u16string benefit_description_;

  // When the benefit is first active and should be displayed.
  base::Time start_time_ = base::Time::Min();

  // When the benefit is no longer active and should no longer be displayed.
  base::Time expiry_time_ = base::Time::Max();
};

// Credit-card-linked benefit that is available to users on any online
// purchases.
class CreditCardFlatRateBenefit : public CreditCardBenefitBase {
 public:
  CreditCardFlatRateBenefit(BenefitId benefit_id,
                            LinkedCardInstrumentId linked_card_instrument_id,
                            std::u16string benefit_description,
                            base::Time start_time,
                            base::Time expiry_time);
  CreditCardFlatRateBenefit(const CreditCardFlatRateBenefit&);
  CreditCardFlatRateBenefit& operator=(const CreditCardFlatRateBenefit&);
  CreditCardFlatRateBenefit(CreditCardFlatRateBenefit&&);
  CreditCardFlatRateBenefit& operator=(CreditCardFlatRateBenefit&&);
  ~CreditCardFlatRateBenefit();

  friend bool operator==(const CreditCardFlatRateBenefit&,
                         const CreditCardFlatRateBenefit&) = default;
  friend auto operator<=>(const CreditCardFlatRateBenefit&,
                          const CreditCardFlatRateBenefit&) = default;

  bool IsValidForWriteFromSync() const;
};

// Credit-card-linked benefit that users receive when making an online purchases
// in specific shopping categories. For example, a benefit the user will receive
// for purchasing a subscription service online with the linked card.
class CreditCardCategoryBenefit : public CreditCardBenefitBase {
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
  CreditCardCategoryBenefit(const CreditCardCategoryBenefit&);
  CreditCardCategoryBenefit& operator=(const CreditCardCategoryBenefit&);
  CreditCardCategoryBenefit(CreditCardCategoryBenefit&&);
  CreditCardCategoryBenefit& operator=(CreditCardCategoryBenefit&&);
  ~CreditCardCategoryBenefit();

  friend bool operator==(const CreditCardCategoryBenefit&,
                         const CreditCardCategoryBenefit&) = default;
  friend auto operator<=>(const CreditCardCategoryBenefit&,
                          const CreditCardCategoryBenefit&) = default;

  bool IsValidForWriteFromSync() const;

  BenefitCategory benefit_category() const { return benefit_category_; }

 private:
  friend class CreditCardCategoryBenefitTestApi;

  // Represents the category of purchases that the benefit can be applied to.
  BenefitCategory benefit_category_ = BenefitCategory::kUnknownBenefitCategory;
};

// Credit-card-linked benefit that users receive when purchasing from specific
// merchant websites.
class CreditCardMerchantBenefit : public CreditCardBenefitBase {
 public:
  CreditCardMerchantBenefit(BenefitId benefit_id,
                            LinkedCardInstrumentId linked_card_instrument_id,
                            std::u16string benefit_description,
                            base::flat_set<url::Origin> merchant_domains,
                            base::Time start_time,
                            base::Time expiry_time);
  CreditCardMerchantBenefit(const CreditCardMerchantBenefit&);
  CreditCardMerchantBenefit& operator=(const CreditCardMerchantBenefit&);
  CreditCardMerchantBenefit(CreditCardMerchantBenefit&&);
  CreditCardMerchantBenefit& operator=(CreditCardMerchantBenefit&&);
  ~CreditCardMerchantBenefit();

  friend bool operator==(const CreditCardMerchantBenefit&,
                         const CreditCardMerchantBenefit&) = default;
  friend auto operator<=>(const CreditCardMerchantBenefit&,
                          const CreditCardMerchantBenefit&) = default;

  bool IsValidForWriteFromSync() const;

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
