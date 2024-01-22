// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field_parser.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"

using base::ASCIIToUTF16;

namespace autofill {

class MerchantPromoCodeFieldParserTest
    : public FormFieldParserTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  MerchantPromoCodeFieldParserTest() : FormFieldParserTestBase(GetParam()) {}
  MerchantPromoCodeFieldParserTest(const MerchantPromoCodeFieldParserTest&) =
      delete;
  MerchantPromoCodeFieldParserTest& operator=(
      const MerchantPromoCodeFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return MerchantPromoCodeFieldParser::Parse(context, scanner);
  }
};

INSTANTIATE_TEST_SUITE_P(
    MerchantPromoCodeFieldParserTest,
    MerchantPromoCodeFieldParserTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

// Match promo(tion|tional)?[-_. ]*code
TEST_P(MerchantPromoCodeFieldParserTest, ParsePromoCode) {
  AddTextFormFieldData("promoCodeField", "Enter promo code here",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_P(MerchantPromoCodeFieldParserTest, ParsePromotionalCode) {
  AddTextFormFieldData("promoCodeField", "Use the promotional code here",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_P(MerchantPromoCodeFieldParserTest, ParsePromoCodeWithPrefixAndSuffix) {
  AddTextFormFieldData("mypromocodefield", "promoCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match coupon[-_. ]*code
TEST_P(MerchantPromoCodeFieldParserTest, ParseCouponCode) {
  AddTextFormFieldData("couponCodeField", "Enter new coupon__code",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match gift[-_. ]*code
TEST_P(MerchantPromoCodeFieldParserTest, ParseGiftCode) {
  AddTextFormFieldData("giftCodeField", "Check out with gift.codes",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Match discount[-_. ]*code
TEST_P(MerchantPromoCodeFieldParserTest, ParseDiscountCode) {
  AddTextFormFieldData("discountCodeField", "Check out with discount-code",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(MerchantPromoCodeFieldParserTest, ParseNonPromoCode) {
  // Regex relies on "promo/coupon/gift" + "code" together.
  AddTextFormFieldData("otherField", "Field for gift card or promo details",
                       UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
