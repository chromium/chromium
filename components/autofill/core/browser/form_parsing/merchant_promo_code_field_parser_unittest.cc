// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/merchant_promo_code_field_parser.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"

using base::ASCIIToUTF16;

namespace autofill {

class MerchantPromoCodeFieldParserTest : public FormFieldParserTestBase,
                                         public testing::Test {
 public:
  MerchantPromoCodeFieldParserTest() = default;
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

// Match promo(tion|tional)?[-_. ]*code
TEST_F(MerchantPromoCodeFieldParserTest, ParsePromoCode) {
  AddTextFormFieldData("promoCodeField", "Enter promo code here",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_F(MerchantPromoCodeFieldParserTest, ParsePromotionalCode) {
  AddTextFormFieldData("promoCodeField", "Use the promotional code here",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Match promo(tion|tional)?[-_. ]*code
TEST_F(MerchantPromoCodeFieldParserTest, ParsePromoCodeWithPrefixAndSuffix) {
  AddTextFormFieldData("mypromocodefield", "promoCodeField",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Match coupon[-_. ]*code
TEST_F(MerchantPromoCodeFieldParserTest, ParseCouponCode) {
  AddTextFormFieldData("couponCodeField", "Enter new coupon__code",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Match gift[-_. ]*code
TEST_F(MerchantPromoCodeFieldParserTest, ParseGiftCode) {
  AddTextFormFieldData("giftCodeField", "Check out with gift.codes",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Match discount[-_. ]*code
TEST_F(MerchantPromoCodeFieldParserTest, ParseDiscountCode) {
  AddTextFormFieldData("discountCodeField", "Check out with discount-code",
                       MERCHANT_PROMO_CODE);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(MerchantPromoCodeFieldParserTest, ParseNonPromoCode) {
  // Regex relies on "promo/coupon/gift" + "code" together.
  AddTextFormFieldData("otherField", "Field for gift card or promo details",
                       UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
