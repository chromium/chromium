// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autocomplete_parsing_util.h"

#include <string>

#include "base/strings/string_piece.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// Tests that parsing a field with autocomplete=`autocomplete` and
// maxlength=`max_length` results in `expected_result`.
struct AutocompleteAttributeTestcase {
  base::StringPiece autocomplete;
  absl::optional<AutocompleteParsingResult> expected_result;
};

class AutocompleteAttributeProcessingUtilTest
    : public testing::TestWithParam<AutocompleteAttributeTestcase> {};

// In general, `ParseAutocompleteAttribute()` returns absl::nullopt if one of
// the tokens cannot be parsed. The exception is the field type, which defaults
// to HtmlFieldType::kUnrecognized.
const AutocompleteAttributeTestcase kAutocompleteTestcases[]{
    // Only the field type:
    {"name", {{"", HtmlFieldMode::kNone, HtmlFieldType::kName}}},
    {"autofill", {{"", HtmlFieldMode::kNone, HtmlFieldType::kUnrecognized}}},
    // autocomplete=off is ignored completely.
    {"off", absl::nullopt},

    // Type hints:
    // They are parsed and validated, but otherwise unused. Type hints are only
    // valid before tel* and email.
    {"home email", {{"", HtmlFieldMode::kNone, HtmlFieldType::kEmail}}},
    {"work email", {{"", HtmlFieldMode::kNone, HtmlFieldType::kEmail}}},
    {"work cc-number", absl::nullopt},
    {"unrecognized_type_hint email", absl::nullopt},

    // Billing and shipping modes:
    {"billing country",
     {{"", HtmlFieldMode::kBilling, HtmlFieldType::kCountryCode}}},
    {"shipping country",
     {{"", HtmlFieldMode::kShipping, HtmlFieldType::kCountryCode}}},
    {"billing unrecognized",
     {{"", HtmlFieldMode::kBilling, HtmlFieldType::kUnrecognized}}},
    {"shipping work tel-local",
     {{"", HtmlFieldMode::kShipping, HtmlFieldType::kTelLocal}}},
    {"unrecognized_mode country", absl::nullopt},
    {"unrecognized_mode unrecognized", absl::nullopt},

    // Sections:
    {"section-one tel", {{"one", HtmlFieldMode::kNone, HtmlFieldType::kTel}}},
    {"section-one shipping tel",
     {{"one", HtmlFieldMode::kShipping, HtmlFieldType::kTel}}},
    {"section-one shipping home tel",
     {{"one", HtmlFieldMode::kShipping, HtmlFieldType::kTel}}},
    {"section- tel", {{"", HtmlFieldMode::kNone, HtmlFieldType::kTel}}},
    {"section tel", absl::nullopt},
    {"no_section tel", absl::nullopt},
    {"no_section work tel", absl::nullopt},
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
    {"hello section-one shipping home tel webauthn", absl::nullopt}};

INSTANTIATE_TEST_SUITE_P(,
                         AutocompleteAttributeProcessingUtilTest,
                         testing::ValuesIn(kAutocompleteTestcases));

TEST_P(AutocompleteAttributeProcessingUtilTest, ParseAutocompleteAttribute) {
  auto test = GetParam();
  SCOPED_TRACE(testing::Message()
               << "autocomplete=\"" << test.autocomplete << "\"");

  FormFieldData field;
  field.autocomplete_attribute = std::string(test.autocomplete);

  auto result = ParseAutocompleteAttribute(field.autocomplete_attribute);
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
