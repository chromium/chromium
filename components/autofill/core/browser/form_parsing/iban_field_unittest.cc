// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/iban_field.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

class IBANFieldTest
    : public FormFieldTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  IBANFieldTest() : FormFieldTestBase(GetParam()) {}
  IBANFieldTest(const IBANFieldTest&) = delete;
  IBANFieldTest& operator=(const IBANFieldTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillParseIBANFields);
  }

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return IBANField::Parse(scanner, page_language, GetActivePatternSource(),
                            /*log_manager=*/nullptr);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    IBANFieldTest,
    IBANFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

#if BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE)
// TODO(crbug.com/1350411): Disabling on linux-chromeos for now as it appears to
// be having trouble enabling this particular feature correctly.
#define MAYBE_ParseIban DISABLED_ParseIban
#else
#define MAYBE_ParseIban ParseIban
#endif
// Match IBAN
TEST_P(IBANFieldTest, MAYBE_ParseIban) {
  AddTextFormFieldData("iban-field", "Enter account number", IBAN_VALUE);

  ClassifyAndVerify(ParseResult::PARSED);
}

#if BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE)
// TODO(crbug.com/1350411): Disabling on linux-chromeos for now as it appears to
// be having trouble enabling this particular feature correctly.
#define MAYBE_ParseIbanBanks DISABLED_ParseIbanBanks
#else
#define MAYBE_ParseIbanBanks ParseIbanBanks
#endif
// Match IBAN on banks
TEST_P(IBANFieldTest, MAYBE_ParseIbanBanks) {
  AddTextFormFieldData("accountNumber", "IBAN*", IBAN_VALUE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(IBANFieldTest, ParseNonIban) {
  AddTextFormFieldData("other-field", "Field for Account Number", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

#if BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE)
// TODO(crbug.com/1350411): Disabling on linux-chromeos for now as it appears to
// be having trouble enabling this particular feature correctly.
#define MAYBE_ParseIbanFlagOff DISABLED_ParseIbanFlagOff
#else
#define MAYBE_ParseIbanFlagOff ParseIbanFlagOff
#endif
TEST_P(IBANFieldTest, MAYBE_ParseIbanFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kAutofillParseIBANFields);
  AddTextFormFieldData("iban-field", "Enter IBAN here", IBAN_VALUE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}
}  // namespace autofill
