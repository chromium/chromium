// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/prediction_improvements_field_parser.h"

#include <memory>

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class PredictionImprovementsFieldParserTest : public FormFieldParserTestBase,
                                              public testing::Test {
 public:
  PredictionImprovementsFieldParserTest() = default;
  PredictionImprovementsFieldParserTest(
      const PredictionImprovementsFieldParserTest&) = delete;
  PredictionImprovementsFieldParserTest& operator=(
      const PredictionImprovementsFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return PredictionImprovementsFieldParser::Parse(context, scanner);
  }
};

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
TEST_F(PredictionImprovementsFieldParserTest, Parse) {
  AddTextFormFieldData("document#", "document#", IMPROVED_PREDICTION);

  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode(""),
                    LanguageCode(""), PatternFile::kPredictionImprovements);
}

TEST_F(PredictionImprovementsFieldParserTest, ParseNonSearchTerm) {
  AddTextFormFieldData("poss", "poss", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed, GeoIpCountryCode(""),
                    LanguageCode(""), PatternFile::kPredictionImprovements);
}
#else
TEST_F(PredictionImprovementsFieldParserTest, Parse) {
  AddTextFormFieldData("document#", "document#", IMPROVED_PREDICTION);

  ClassifyAndVerify(ParseResult::kNotParsed, GeoIpCountryCode(""),
                    LanguageCode(""), PatternFile::kLegacy);
}
#endif

}  // namespace autofill
