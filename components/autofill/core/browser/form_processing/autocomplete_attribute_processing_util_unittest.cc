// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/autocomplete_attribute_processing_util.h"

#include <string>

#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// Tests that parsing a field with autocomplete=`autocomplete` and
// maxlength=`max_length` results in `expected_result`.
struct AutocompleteAttributeTestcase {
  base::StringPiece autocomplete;
  absl::optional<AutocompleteParsingResult> expected_result;
  int max_length = 0;
};

class AutocompleteAttributeProcessingUtilTest
    : public testing::TestWithParam<AutocompleteAttributeTestcase> {};

// In general, `ParseAutocompleteAttribute()` returns absl::nullopt if one of
// the tokens cannot be parsed. The exception is the field type, which defaults
// to HTML_TYPE_UNRECOGNIZED.
const AutocompleteAttributeTestcase kAutocompleteTestcases[]{
    // Only the field type:
    {"name", {{"", HTML_MODE_NONE, HTML_TYPE_NAME}}},
    {"autofill", {{"", HTML_MODE_NONE, HTML_TYPE_UNRECOGNIZED}}},
    // autocomplete=off is ignored completely.
    {"off", absl::nullopt},

    // Rationalization based on the field's max_length is done.
    {"cc-exp-year", {{"", HTML_MODE_NONE, HTML_TYPE_CREDIT_CARD_EXP_YEAR}}},
    {"cc-exp-year",
     {{"", HTML_MODE_NONE, HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR}},
     /*max_length=*/2},

    // Type hints:
    // They are parsed and validated, but otherwise unused. Type hints are only
    // valid before tel* and email.
    {"home email", {{"", HTML_MODE_NONE, HTML_TYPE_EMAIL}}},
    {"work email", {{"", HTML_MODE_NONE, HTML_TYPE_EMAIL}}},
    {"work cc-number", absl::nullopt},
    {"unrecognized_type_hint email", absl::nullopt},

    // Billing and shipping modes:
    {"billing country", {{"", HTML_MODE_BILLING, HTML_TYPE_COUNTRY_CODE}}},
    {"shipping country", {{"", HTML_MODE_SHIPPING, HTML_TYPE_COUNTRY_CODE}}},
    {"billing unrecognized", {{"", HTML_MODE_BILLING, HTML_TYPE_UNRECOGNIZED}}},
    {"shipping work tel-local",
     {{"", HTML_MODE_SHIPPING, HTML_TYPE_TEL_LOCAL}}},
    {"unrecognized_mode country", absl::nullopt},
    {"unrecognized_mode unrecognized", absl::nullopt},

    // Sections:
    {"section-one tel", {{"one", HTML_MODE_NONE, HTML_TYPE_TEL}}},
    {"section-one shipping tel", {{"one", HTML_MODE_SHIPPING, HTML_TYPE_TEL}}},
    {"section-one shipping home tel",
     {{"one", HTML_MODE_SHIPPING, HTML_TYPE_TEL}}},
    {"section- tel", {{"", HTML_MODE_NONE, HTML_TYPE_TEL}}},
    {"section tel", absl::nullopt},
    {"no_section tel", absl::nullopt},
    {"no_section work tel", absl::nullopt},

    // "webauthn" shouldn't prevent parsing, but is otherwise ignored.
    {"name webauthn", {{"", HTML_MODE_NONE, HTML_TYPE_NAME}}},
    {"section-one shipping home tel webauthn",
     {{"one", HTML_MODE_SHIPPING, HTML_TYPE_TEL}}},
    {"webauthn", absl::nullopt},

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
  if (test.max_length)
    field.max_length = test.max_length;

  auto result = ParseAutocompleteAttribute(AutofillField(field));
  ASSERT_EQ(result.has_value(), test.expected_result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(result->section, test.expected_result->section);
    EXPECT_EQ(result->mode, test.expected_result->mode);
    EXPECT_EQ(result->field_type, test.expected_result->field_type);
  }
}

}  // namespace autofill
