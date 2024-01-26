// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field_parser.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

using base::ASCIIToUTF16;

namespace autofill {

class PriceFieldParserTest
    : public FormFieldParserTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  PriceFieldParserTest() : FormFieldParserTestBase(GetParam()) {}
  PriceFieldParserTest(const PriceFieldParserTest&) = delete;
  PriceFieldParserTest& operator=(const PriceFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return PriceFieldParser::Parse(context, scanner);
  }
};

INSTANTIATE_TEST_SUITE_P(
    PriceFieldParserTest,
    PriceFieldParserTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(PriceFieldParserTest, ParsePrice) {
  AddTextFormFieldData("userPrice", "name your price", PRICE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(PriceFieldParserTest, ParseNonPrice) {
  AddTextFormFieldData("firstName", "Name", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
