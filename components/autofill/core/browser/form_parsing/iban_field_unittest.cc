// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/iban_field.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

namespace autofill {

class IbanFieldTest
    : public FormFieldTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  IbanFieldTest() : FormFieldTestBase(GetParam()) {}
  IbanFieldTest(const IbanFieldTest&) = delete;
  IbanFieldTest& operator=(const IbanFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const GeoIpCountryCode& client_country,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return IbanField::Parse(scanner, client_country, page_language,
                            *GetActivePatternSource(),
                            /*log_manager=*/nullptr);
  }
};

INSTANTIATE_TEST_SUITE_P(
    IbanFieldTest,
    IbanFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

// Match IBAN
TEST_P(IbanFieldTest, ParseIban) {
  AddTextFormFieldData("iban-field", "Enter account number", IBAN_VALUE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(IbanFieldTest, ParseIbanBanks) {
  AddTextFormFieldData("accountNumber", "IBAN*", IBAN_VALUE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(IbanFieldTest, ParseNonIban) {
  AddTextFormFieldData("other-field", "Field for Account Number", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
