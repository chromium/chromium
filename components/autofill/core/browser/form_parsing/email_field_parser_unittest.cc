// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/email_field_parser.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class EmailFieldParserTest : public FormFieldParserTestBase,
                             public ::testing::Test {
 public:
  EmailFieldParserTest() = default;
  EmailFieldParserTest(const EmailFieldParserTest&) = delete;
  EmailFieldParserTest& operator=(const EmailFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return EmailFieldParser::Parse(context, scanner);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillParseEmailLabelAndPlaceholder};
};

// Tests that a field whose label has the format of an email address is parsed
// as `EMAIL_ADDRESS`.
TEST_F(EmailFieldParserTest, ParseEmailAddressLabel) {
  AddTextFormFieldData(/*name=*/"username", /*label=*/"some@foo.com",
                       EMAIL_ADDRESS);
  ClassifyAndVerify(ParseResult::kParsed);
}

// Tests that a field whose placeholder has the format of an email address is
// parsed as `EMAIL_ADDRESS`.
TEST_F(EmailFieldParserTest, ParseEmailAddressPlaceholder) {
  AddFormFieldData(
      FormControlType::kInputText, /*name=*/"username", /*label=*/"",
      /*placeholder=*/"some@foo.com", /*max_length=*/0, EMAIL_ADDRESS);
  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(EmailFieldParserTest, ParseDomainLabel) {
  AddTextFormFieldData(/*name=*/"username", /*label=*/"foo.com", EMAIL_ADDRESS);
  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_F(EmailFieldParserTest, ParseDomainPlaceholder) {
  AddFormFieldData(FormControlType::kInputText, /*name=*/"username",
                   /*label=*/"some@",
                   /*placeholder=*/"foo.com", /*max_length=*/0, EMAIL_ADDRESS);
  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
