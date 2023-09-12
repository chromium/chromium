// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"

using base::ASCIIToUTF16;

namespace autofill {

class MerchantPromoCodeFieldTest
    : public FormFieldTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  MerchantPromoCodeFieldTest() : FormFieldTestBase(GetParam()) {}
  MerchantPromoCodeFieldTest(const MerchantPromoCodeFieldTest&) = delete;
  MerchantPromoCodeFieldTest& operator=(const MerchantPromoCodeFieldTest&) =
      delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const GeoIpCountryCode& client_country,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return MerchantPromoCodeField::Parse(scanner, client_country, page_language,
                                         *GetActivePatternSource(),
                                         /*log_manager=*/nullptr);
  }
};

INSTANTIATE_TEST_SUITE_P(
    MerchantPromoCodeFieldTest,
    MerchantPromoCodeFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

// Match promo(tion|tional)?[-_. ]*code
TEST_P(MerchantPromoCodeFieldTest, ParsePromoCode) {
  AddTextFormFieldData("promoCodeField", "Enter promo code here",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_P(MerchantPromoCodeFieldTest, ParsePromotionalCode) {
  AddTextFormFieldData("promoCodeField", "Use the promotional code here",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_P(MerchantPromoCodeFieldTest, ParsePromoCodeWithPrefixAndSuffix) {
  AddTextFormFieldData("mypromocodefield", "promoCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match coupon[-_. ]*code
TEST_P(MerchantPromoCodeFieldTest, ParseCouponCode) {
  AddTextFormFieldData("couponCodeField", "Enter new coupon__code",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match gift[-_. ]*code
TEST_P(MerchantPromoCodeFieldTest, ParseGiftCode) {
  AddTextFormFieldData("giftCodeField", "Check out with gift.codes",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match discount[-_. ]*code
TEST_P(MerchantPromoCodeFieldTest, ParseDiscountCode) {
  AddTextFormFieldData("discountCodeField", "Check out with discount-code",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(MerchantPromoCodeFieldTest, ParseNonPromoCode) {
  // Regex relies on "promo/coupon/gift" + "code" together.
  AddTextFormFieldData("otherField", "Field for gift card or promo details",
                       UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
