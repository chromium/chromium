// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/numeric_quantity_field.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

namespace autofill {

class NumericQuantityFieldTest
    : public FormFieldTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  explicit NumericQuantityFieldTest() : FormFieldTestBase(GetParam()) {}
  NumericQuantityFieldTest(const NumericQuantityFieldTest&) = delete;
  NumericQuantityFieldTest& operator=(const NumericQuantityFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return NumericQuantityField::Parse(scanner, page_language,
                                       GetActivePatternSource(),
                                       /*log_manager=*/nullptr);
  }
};

INSTANTIATE_TEST_SUITE_P(
    NumericQuantityFieldTest,
    NumericQuantityFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(NumericQuantityFieldTest, ParseNumericQuantity) {
  AddTextFormFieldData("quantity", "quantity", NUMERIC_QUANTITY);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(NumericQuantityFieldTest, ParseNonNumericQuantity) {
  AddTextFormFieldData("name", "Name", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
