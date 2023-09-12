// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/standalone_cvc_field.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

class StandaloneCvcFieldTest
    : public FormFieldTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  StandaloneCvcFieldTest() : FormFieldTestBase(GetParam()) {}
  StandaloneCvcFieldTest(const StandaloneCvcFieldTest&) = delete;
  StandaloneCvcFieldTest& operator=(const StandaloneCvcFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const GeoIpCountryCode& client_country,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return StandaloneCvcField::Parse(scanner, client_country, page_language,
                                     *GetActivePatternSource(),
                                     /*log_manager=*/nullptr);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    StandaloneCvcFieldTest,
    StandaloneCvcFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

// Match standalone cvc.
TEST_P(StandaloneCvcFieldTest, ParseStandaloneCvc) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("cvc", "CVC:", CREDIT_CARD_STANDALONE_VERIFICATION_CODE);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Do not parse non cvc standalone fields.
TEST_P(StandaloneCvcFieldTest, ParseNonStandaloneCvc) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("other-field", "Other Field:", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

// Do not parse when standalone cvc flag is disabled.
TEST_P(StandaloneCvcFieldTest, ParseStandaloneCvcFlagOff) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("cvc", "CVC:", CREDIT_CARD_STANDALONE_VERIFICATION_CODE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

// Do not parse gift card as standalone cvc fields.
TEST_P(StandaloneCvcFieldTest, NotParseGiftCardAsStandaloneCvc) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("gift-card", "Gift Card Pin:", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
