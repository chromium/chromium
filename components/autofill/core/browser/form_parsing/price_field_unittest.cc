// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

using base::ASCIIToUTF16;

namespace autofill {

class PriceFieldTest : public FormFieldTest {
 public:
  PriceFieldTest() = default;
  PriceFieldTest(const PriceFieldTest&) = delete;
  PriceFieldTest& operator=(const PriceFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return PriceField::Parse(scanner, page_language, nullptr);
  }
};

TEST_F(PriceFieldTest, ParsePrice) {
  AddTextFormFieldData("name your price", "userPrice", PRICE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_F(PriceFieldTest, ParseNonPrice) {
  AddTextFormFieldData("firstName", "Name", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
