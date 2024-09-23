// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/standalone_cvc_field_parser.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

class StandaloneCvcFieldParserTest : public FormFieldParserTestBase,
                                     public testing::Test {
 public:
  StandaloneCvcFieldParserTest() = default;
  StandaloneCvcFieldParserTest(const StandaloneCvcFieldParserTest&) = delete;
  StandaloneCvcFieldParserTest& operator=(const StandaloneCvcFieldParserTest&) =
      delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return StandaloneCvcFieldParser::Parse(context, scanner);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Match standalone cvc.
TEST_F(StandaloneCvcFieldParserTest, ParseStandaloneCvc) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("cvc", "CVC:", CREDIT_CARD_STANDALONE_VERIFICATION_CODE);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Do not parse non cvc standalone fields.
TEST_F(StandaloneCvcFieldParserTest, ParseNonStandaloneCvc) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("other-field", "Other Field:", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

// Do not parse when standalone cvc flag is disabled.
TEST_F(StandaloneCvcFieldParserTest, ParseStandaloneCvcFlagOff) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("cvc", "CVC:", CREDIT_CARD_STANDALONE_VERIFICATION_CODE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

// Do not parse gift card as standalone cvc fields.
TEST_F(StandaloneCvcFieldParserTest, NotParseGiftCardAsStandaloneCvc) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("gift-card", "Gift Card Pin:", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
