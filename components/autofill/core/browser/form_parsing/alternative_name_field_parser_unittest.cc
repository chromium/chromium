// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/alternative_name_field_parser.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

class AlternativeNameFieldParserTest : public FormFieldParserTestBase,
                                       public testing::Test {
 public:
  AlternativeNameFieldParserTest() = default;
  AlternativeNameFieldParserTest(const AlternativeNameFieldParserTest&) =
      delete;
  AlternativeNameFieldParserTest& operator=(
      const AlternativeNameFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return AlternativeNameFieldParser::Parse(context, scanner);
  }

 private:
  base::test::ScopedFeatureList scoped_features{
      features::kAutofillSupportPhoneticNameForJP};
};

TEST_F(AlternativeNameFieldParserTest, FamilyGivenPhoneticName) {
  AddTextFormFieldData("family-phonetic-name", "family-phonetic-name",
                       ALTERNATIVE_FAMILY_NAME);
  AddTextFormFieldData("given-phonetic-name", "given-phonetic-name",
                       ALTERNATIVE_GIVEN_NAME);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(AlternativeNameFieldParserTest, FullPhoneticName) {
  AddTextFormFieldData("full-phonetic-name", "full-phonetic-name",
                       ALTERNATIVE_FULL_NAME);

  ClassifyAndVerify(ParseResult::kParsed);
}

}  // namespace

}  // namespace autofill
