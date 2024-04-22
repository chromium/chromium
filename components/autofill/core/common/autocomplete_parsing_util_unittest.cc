// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autocomplete_parsing_util.h"

#include <optional>
#include <string>

#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Tests that parsing a field with autocomplete=`autocomplete` and
// maxlength=`max_length` results in `expected_result`.
struct AutocompleteAttributeTestcase {
  std::string_view autocomplete;
  std::optional<AutocompleteParsingResult> expected_result;
};

class AutocompleteAttributeProcessingUtilTest
    : public testing::TestWithParam<AutocompleteAttributeTestcase> {};

// In general, `ParseAutocompleteAttribute()` returns std::nullopt if one of
// the tokens cannot be parsed. The exception is the field type, which defaults
// to HtmlFieldType::kUnrecognized.
const AutocompleteAttributeTestcase kAutocompleteTestcases[]{
    // Only the field type:
    {"name", {{"", HtmlFieldMode::kNone, HtmlFieldType::kName}}},
    {"autofill", {{"", HtmlFieldMode::kNone, HtmlFieldType::kUnrecognized}}},
    // autocomplete=off is ignored completely.
    {"off", std::nullopt},

    // Type hints:
    // They are parsed and validated, but otherwise unused. Type hints are only
    // valid before tel* and email.
    {"home email", {{"", HtmlFieldMode::kNone, HtmlFieldType::kEmail}}},
    {"work email", {{"", HtmlFieldMode::kNone, HtmlFieldType::kEmail}}},
    {"work cc-number", std::nullopt},
    {"unrecognized_type_hint email", std::nullopt},

    // Billing and shipping modes:
    {"billing country",
     {{"", HtmlFieldMode::kBilling, HtmlFieldType::kCountryCode}}},
    {"shipping country",
     {{"", HtmlFieldMode::kShipping, HtmlFieldType::kCountryCode}}},
    {"billing unrecognized",
     {{"", HtmlFieldMode::kBilling, HtmlFieldType::kUnrecognized}}},
    {"shipping work tel-local",
     {{"", HtmlFieldMode::kShipping, HtmlFieldType::kTelLocal}}},
    {"unrecognized_mode country", std::nullopt},
    {"unrecognized_mode unrecognized", std::nullopt},

    // Sections:
    {"section-one tel", {{"one", HtmlFieldMode::kNone, HtmlFieldType::kTel}}},
    {"section-one shipping tel",
     {{"one", HtmlFieldMode::kShipping, HtmlFieldType::kTel}}},
    {"section-one shipping home tel",
     {{"one", HtmlFieldMode::kShipping, HtmlFieldType::kTel}}},
    {"section- tel", {{"", HtmlFieldMode::kNone, HtmlFieldType::kTel}}},
    {"section tel", std::nullopt},
    {"no_section tel", std::nullopt},
    {"no_section work tel", std::nullopt},
    {"section-random",
     {{"", HtmlFieldMode::kNone, HtmlFieldType::kUnrecognized}}},

    // "webauthn" token:
    {"name webauthn",
     {{"", HtmlFieldMode::kNone, HtmlFieldType::kName, /*webauthn=*/true}}},
    {"section-one shipping home tel webauthn",
     {{"one", HtmlFieldMode::kShipping, HtmlFieldType::kTel,
       /*webauthn=*/true}}},
    {"webauthn",
     {{"", HtmlFieldMode::kNone, HtmlFieldType::kUnspecified,
       /*webauthn=*/true}}},

    // Too many tokens.
    {"hello section-one shipping home tel webauthn", std::nullopt}};

INSTANTIATE_TEST_SUITE_P(,
                         AutocompleteAttributeProcessingUtilTest,
                         testing::ValuesIn(kAutocompleteTestcases));

TEST_P(AutocompleteAttributeProcessingUtilTest, ParseAutocompleteAttribute) {
  auto test = GetParam();
  SCOPED_TRACE(testing::Message()
               << "autocomplete=\"" << test.autocomplete << "\"");

  FormFieldData field;
  field.set_autocomplete_attribute(std::string(test.autocomplete));

  auto result = ParseAutocompleteAttribute(field.autocomplete_attribute());
  EXPECT_EQ(result, test.expected_result);
}

TEST_F(AutocompleteAttributeProcessingUtilTest,
       IsAutocompleteTypeWrongButWellIntended_FalseOnIgnoredValues) {
  EXPECT_FALSE(IsAutocompleteTypeWrongButWellIntended("on"));
  EXPECT_FALSE(IsAutocompleteTypeWrongButWellIntended("off"));
  EXPECT_FALSE(IsAutocompleteTypeWrongButWellIntended("false"));
}

TEST_F(
    AutocompleteAttributeProcessingUtilTest,
    IsAutocompleteTypeWrongButWellIntended_FalseOnSubstringMatchWithKnownValue) {
  EXPECT_FALSE(IsAutocompleteTypeWrongButWellIntended("new-password"));
  EXPECT_FALSE(IsAutocompleteTypeWrongButWellIntended("one-time-code"));
}

TEST_F(AutocompleteAttributeProcessingUtilTest,
       IsAutocompleteTypeWrongButWellIntended_TrueOnPossibleTypo) {
  EXPECT_TRUE(IsAutocompleteTypeWrongButWellIntended("familyname"));
}

TEST_F(AutocompleteAttributeProcessingUtilTest,
       IsAutocompleteTypeWrongButWellIntended_FalseOnDisableKeywordFound) {
  EXPECT_FALSE(IsAutocompleteTypeWrongButWellIntended("off-familyname"));
}

}  // namespace autofill
