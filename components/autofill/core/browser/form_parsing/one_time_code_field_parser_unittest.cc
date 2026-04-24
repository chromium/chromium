// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/one_time_code_field_parser.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class OneTimeCodeFieldParserTest : public FormFieldParserTestBase,
                                   public ::testing::Test {
 public:
  OneTimeCodeFieldParserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableOneTimeCodeHeuristics);
  }
  OneTimeCodeFieldParserTest(const OneTimeCodeFieldParserTest&) = delete;
  OneTimeCodeFieldParserTest& operator=(const OneTimeCodeFieldParserTest&) =
      delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner& scanner) override {
    return OneTimeCodeFieldParser::Parse(context, scanner);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OneTimeCodeFieldParserTest, ParseOneTimeCode) {
  AddTextFormFieldData(/*name=*/"otp", /*label=*/"Enter your code",
                       ONE_TIME_CODE);
  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(OneTimeCodeFieldParserTest, ParseTwoFactor) {
  AddTextFormFieldData(/*name=*/"2fa", /*label=*/"Verification code",
                       ONE_TIME_CODE);
  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(OneTimeCodeFieldParserTest, ParseSmsOtp) {
  AddTextFormFieldData(/*name=*/"sms_otp", /*label=*/"SMS Code", ONE_TIME_CODE);
  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(OneTimeCodeFieldParserTest, NonOtp) {
  AddTextFormFieldData(/*name=*/"username", /*label=*/"Username", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
