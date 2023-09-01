// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

using base::ASCIIToUTF16;

namespace autofill {

class PriceFieldTest
    : public FormFieldTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  PriceFieldTest() : FormFieldTestBase(GetParam()) {}
  PriceFieldTest(const PriceFieldTest&) = delete;
  PriceFieldTest& operator=(const PriceFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return PriceField::Parse(scanner, page_language, *GetActivePatternSource(),
                             /*log_manager=*/nullptr);
  }
};

INSTANTIATE_TEST_SUITE_P(
    PriceFieldTest,
    PriceFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(PriceFieldTest, ParsePrice) {
  AddTextFormFieldData("userPrice", "name your price", PRICE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(PriceFieldTest, ParseNonPrice) {
  AddTextFormFieldData("firstName", "Name", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
