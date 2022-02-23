// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"

using base::ASCIIToUTF16;

namespace autofill {

class MerchantPromoCodeFieldTest : public FormFieldTest {
 public:
  MerchantPromoCodeFieldTest() = default;
  MerchantPromoCodeFieldTest(const MerchantPromoCodeFieldTest&) = delete;
  MerchantPromoCodeFieldTest& operator=(const MerchantPromoCodeFieldTest&) =
      delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillParseMerchantPromoCodeFields);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return MerchantPromoCodeField::Parse(scanner, page_language, nullptr);
  }
};

// Match promo(tion|tional)?[-_. ]*code
TEST_F(MerchantPromoCodeFieldTest, ParsePromoCode) {
  AddTextFormFieldData("Enter promo code here", "promoCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_F(MerchantPromoCodeFieldTest, ParsePromotionalCode) {
  AddTextFormFieldData("Use the promotional code here", "promoCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_F(MerchantPromoCodeFieldTest, ParsePromoCodeWithPrefixAndSuffix) {
  AddTextFormFieldData("mypromocodefield", "promoCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match coupon[-_. ]*code
TEST_F(MerchantPromoCodeFieldTest, ParseCouponCode) {
  AddTextFormFieldData("Enter new coupon__code", "couponCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match gift[-_. ]*code
TEST_F(MerchantPromoCodeFieldTest, ParseGiftCode) {
  AddTextFormFieldData("Check out with gift.codes", "giftCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match discount[-_. ]*code
TEST_F(MerchantPromoCodeFieldTest, ParseDiscountCode) {
  AddTextFormFieldData("Check out with discount-code", "discountCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(MerchantPromoCodeFieldTest, ParseNonPromoCode) {
  // Regex relies on "promo/coupon/gift" + "code" together.
  AddTextFormFieldData("Field for gift card or promo details", "otherField",
                       UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_F(MerchantPromoCodeFieldTest, ParsePromoCodeFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillParseMerchantPromoCodeFields);
  AddTextFormFieldData("Enter promo code here", "promoCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
