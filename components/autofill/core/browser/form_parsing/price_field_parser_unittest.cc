// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field_parser.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

using base::ASCIIToUTF16;

namespace autofill {

class PriceFieldParserTest : public FormFieldParserTestBase,
                             public testing::Test {
 public:
  PriceFieldParserTest() = default;
  PriceFieldParserTest(const PriceFieldParserTest&) = delete;
  PriceFieldParserTest& operator=(const PriceFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return PriceFieldParser::Parse(context, scanner);
  }
};

TEST_F(PriceFieldParserTest, ParsePrice) {
  AddTextFormFieldData("userPrice", "name your price", PRICE);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(PriceFieldParserTest, ParseNonPrice) {
  AddTextFormFieldData("firstName", "Name", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
