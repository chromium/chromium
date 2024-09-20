// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/iban_field_parser.h"

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

namespace autofill {

class IbanFieldParserTest : public FormFieldParserTestBase,
                            public testing::Test {
 public:
  IbanFieldParserTest() = default;
  IbanFieldParserTest(const IbanFieldParserTest&) = delete;
  IbanFieldParserTest& operator=(const IbanFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return IbanFieldParser::Parse(context, scanner);
  }
};

// Match IBAN
TEST_F(IbanFieldParserTest, ParseIban) {
  AddTextFormFieldData("iban-field", "Enter account number", IBAN_VALUE);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(IbanFieldParserTest, ParseIbanBanks) {
  AddTextFormFieldData("accountNumber", "IBAN*", IBAN_VALUE);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(IbanFieldParserTest, ParseNonIban) {
  AddTextFormFieldData("other-field", "Field for Account Number", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
