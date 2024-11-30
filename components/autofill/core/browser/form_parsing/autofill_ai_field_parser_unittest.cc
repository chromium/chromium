// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_ai_field_parser.h"

#include <memory>

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutofillAiFieldParserTest : public FormFieldParserTestBase,
                                  public testing::Test {
 public:
  AutofillAiFieldParserTest() = default;
  AutofillAiFieldParserTest(const AutofillAiFieldParserTest&) = delete;
  AutofillAiFieldParserTest& operator=(const AutofillAiFieldParserTest&) =
      delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return AutofillAiFieldParser::Parse(context, scanner);
  }
};

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
TEST_F(AutofillAiFieldParserTest, Parse) {
  AddTextFormFieldData("document#", "document#", IMPROVED_PREDICTION);

  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode(""),
                    LanguageCode(""), PatternFile::kAutofillAi);
}

TEST_F(AutofillAiFieldParserTest, ParseNonSearchTerm) {
  AddTextFormFieldData("poss", "poss", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed, GeoIpCountryCode(""),
                    LanguageCode(""), PatternFile::kAutofillAi);
}
#else
TEST_F(AutofillAiFieldParserTest, Parse) {
  AddTextFormFieldData("document#", "document#", IMPROVED_PREDICTION);

  ClassifyAndVerify(ParseResult::kNotParsed, GeoIpCountryCode(""),
                    LanguageCode(""), PatternFile::kLegacy);
}
#endif

}  // namespace autofill
