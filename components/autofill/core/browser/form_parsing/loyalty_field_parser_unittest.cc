// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/loyalty_field_parser.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

namespace autofill {

class LoyaltyFieldParserTest : public FormFieldParserTestBase,
                               public testing::Test {
 public:
  LoyaltyFieldParserTest() = default;
  LoyaltyFieldParserTest(const LoyaltyFieldParserTest&) = delete;
  LoyaltyFieldParserTest& operator=(const LoyaltyFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner& scanner) override {
    return LoyaltyFieldParser::Parse(context, scanner);
  }

  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableLoyaltyCardsFilling};
};

TEST_F(LoyaltyFieldParserTest, ParseFrequentFlyerField) {
  AddTextFormFieldData("frequent-flyer-number", "Enter account number",
                       LOYALTY_MEMBERSHIP_ID);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(LoyaltyFieldParserTest, ParseLoyaltyNumberField) {
  AddTextFormFieldData("loyaltyNumber", "Loyalty Number",
                       LOYALTY_MEMBERSHIP_ID);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(LoyaltyFieldParserTest, ParseNonLoyaltyCardField) {
  AddTextFormFieldData("other-field", "Field for Account Number", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
