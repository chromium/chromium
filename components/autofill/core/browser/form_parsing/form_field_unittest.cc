// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class FormFieldTest
    : public FormFieldTestBase,
      public ::testing::TestWithParam<std::tuple<
          PatternProviderFeatureState,
          /* features::kAutofillMin3FieldTypesForLocalHeuristics */ bool>> {
 public:
  FormFieldTest();
  FormFieldTest(const FormFieldTest&) = delete;
  FormFieldTest& operator=(const FormFieldTest&) = delete;

  const PatternProviderFeatureState& pattern_provider_feature_state() const {
    return std::get<0>(GetParam());
  }
  bool require_min_3_field_types_for_local_heuristics() const {
    return std::get<1>(GetParam());
  }

 protected:
  // Parses all added fields using `ParseFormFields`.
  // Returns the number of fields parsed.
  int ParseFormFields() {
    FormField::ParseFormFields(list_, LanguageCode(""),
                               /*is_form_tag=*/true, GetActivePatternSource(),
                               field_candidates_map_,
                               /*log_manager=*/nullptr);
    return field_candidates_map_.size();
  }

  // Like `ParseFormFields()`, but using `ParseSingleFieldForms()` instead.
  int ParseSingleFieldForms() {
    FormField::ParseSingleFieldForms(
        list_, LanguageCode(""),
        /*is_form_tag=*/true, GetActivePatternSource(), field_candidates_map_);
    return field_candidates_map_.size();
  }

  // FormFieldTestBase:
  // This function is unused in these unit tests, because FormField is not a
  // parser itself, but the infrastructure combining them.
  std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                   const LanguageCode& page_language) override {
    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

FormFieldTest::FormFieldTest()
    : FormFieldTestBase(pattern_provider_feature_state()) {
  if (require_min_3_field_types_for_local_heuristics()) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillMin3FieldTypesForLocalHeuristics);
  } else {
    scoped_feature_list_.InitAndDisableFeature(
        features::kAutofillMin3FieldTypesForLocalHeuristics);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FormFieldTest,
    FormFieldTest,
    ::testing::Combine(::testing::ValuesIn(PatternProviderFeatureState::All()),
                       ::testing::Values(true, false)));

struct MatchTestCase {
  std::u16string label;
  std::vector<std::u16string> positive_patterns;
  std::vector<std::u16string> negative_patterns;
};

class MatchTest : public testing::TestWithParam<MatchTestCase> {};

const MatchTestCase kMatchTestCases[]{
    // Empty strings match empty patterns, but not non-empty ones.
    {u"", {u"", u"^$"}, {u"a"}},
    // Non-empty strings don't match empty patterns.
    {u"a", {u""}, {u"^$"}},
    // Beginning and end of the line and exact matches.
    {u"head_tail",
     {u"^head", u"tail$", u"^head_tail$"},
     {u"head$", u"^tail", u"^head$", u"^tail$"}},
    // Escaped dots.
    // Note: The unescaped "." characters are wild cards.
    {u"m.i.", {u"m.i.", u"m\\.i\\."}},
    {u"mXiX", {u"m.i."}, {u"m\\.i\\."}},
    // Repetition.
    {u"headtail", {u"head.*tail"}, {u"head.+tail"}},
    {u"headXtail", {u"head.*tail", u"head.+tail"}},
    {u"headXXXtail", {u"head.*tail", u"head.+tail"}},
    // Alternation.
    {u"head_tail", {u"head|other", u"tail|other"}, {u"bad|good"}},
    // Case sensitivity.
    {u"xxxHeAd_tAiLxxx", {u"head_tail"}},
    // Word boundaries.
    {u"contains word:", {u"\\bword\\b"}, {u"\\bcon\\b"}},
    // Make sure the circumflex in 'crêpe' is not treated as a word boundary.
    {u"crêpe", {}, {u"\\bcr\\b"}}};

INSTANTIATE_TEST_SUITE_P(FormFieldTest,
                         MatchTest,
                         testing::ValuesIn(kMatchTestCases));

TEST_P(MatchTest, Match) {
  const auto& [label, positive_patterns, negative_patterns] = GetParam();
  constexpr MatchParams kMatchLabel{{MatchAttribute::kLabel}, {}};
  AutofillField field;
  SCOPED_TRACE("label = " + base::UTF16ToUTF8(label));
  field.label = label;
  field.set_parseable_label(label);
  for (const auto& pattern : positive_patterns) {
    SCOPED_TRACE("positive_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_TRUE(FormField::MatchForTesting(&field, pattern, kMatchLabel));
  }
  for (const auto& pattern : negative_patterns) {
    SCOPED_TRACE("negative_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_FALSE(FormField::MatchForTesting(&field, pattern, kMatchLabel));
  }
}

// Test that we ignore checkable elements.
TEST_P(FormFieldTest, ParseFormFieldsIgnoreCheckableElements) {
  AddFormFieldData("checkbox", "", "Is PO Box", UNKNOWN_TYPE);
  // Add 3 dummy fields to reach kMinRequiredFieldsForHeuristics = 3.
  AddTextFormFieldData("", "Address line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("", "Address line 2", ADDRESS_HOME_LINE2);
  AddTextFormFieldData("", "Address line 3", ADDRESS_HOME_LINE3);
  EXPECT_EQ(3, ParseFormFields());
  TestClassificationExpectations();
}

// Test that the minimum number of required fields for the heuristics considers
// whether a field is actually fillable.
TEST_P(FormFieldTest, ParseFormFieldsEnforceMinFillableFields) {
  AddTextFormFieldData("", "Address line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("", "Address line 2", ADDRESS_HOME_LINE2);
  AddTextFormFieldData("", "Search", SEARCH_TERM);
  // We don't parse the form because search fields are not fillable (therefore,
  // the form has only 2 fillable fields).
  EXPECT_EQ(0, ParseFormFields());
}

// Tests that the `parseable_name()` is parsed as an autocomplete type.
TEST_P(FormFieldTest, ParseNameAsAutocompleteType) {
  base::test::ScopedFeatureList autocomplete_feature;
  autocomplete_feature.InitAndEnableFeature(
      features::kAutofillParseNameAsAutocompleteType);

  AddTextFormFieldData("given-name", "", NAME_FIRST);
  AddTextFormFieldData("family-name", "", NAME_LAST);
  AddTextFormFieldData("cc-exp-month", "", CREDIT_CARD_EXP_MONTH);
  // The label is not parsed as an autocomplete type.
  AddTextFormFieldData("", "cc-exp-month", UNKNOWN_TYPE);
  EXPECT_EQ(3, ParseFormFields());
  TestClassificationExpectations();
}

// Test that the parseable label is used when the feature is enabled.
TEST_P(FormFieldTest, TestParseableLabels) {
  AddTextFormFieldData("", "not a parseable label", UNKNOWN_TYPE);
  AutofillField* autofill_field = list_.back().get();
  autofill_field->set_parseable_label(u"First Name");

  constexpr MatchParams kMatchLabel{{MatchAttribute::kLabel}, {}};
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    EXPECT_TRUE(
        FormField::MatchForTesting(autofill_field, u"First Name", kMatchLabel));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    EXPECT_FALSE(
        FormField::MatchForTesting(autofill_field, u"First Name", kMatchLabel));
  }
}

// Tests that `ParseSingleFieldForms` is called as part of `ParseFormFields`.
TEST_P(FormFieldTest, ParseSingleFieldFormsInsideParseFormField) {
  AddTextFormFieldData("", "Phone", PHONE_HOME_WHOLE_NUMBER);
  AddTextFormFieldData("", "Email", EMAIL_ADDRESS);
  AddTextFormFieldData("", "Promo code", MERCHANT_PROMO_CODE);

  // `ParseSingleFieldForms` should detect the promo code.
  EXPECT_EQ(3, ParseFormFields());
  TestClassificationExpectations();
}

// Test that `ParseSingleFieldForms` parses single field promo codes.
TEST_P(FormFieldTest, ParseFormFieldsForSingleFieldPromoCode) {
  // Parse single field promo code.
  AddTextFormFieldData("", "Promo code", MERCHANT_PROMO_CODE);
  EXPECT_EQ(1, ParseSingleFieldForms());
  TestClassificationExpectations();

  // Don't parse other fields.
  // UNKNOWN_TYPE is used as the expected type, which prevents it from being
  // part of the expectations in `TestClassificationExpectations()`.
  AddTextFormFieldData("", "Address line 1", UNKNOWN_TYPE);
  EXPECT_EQ(1, ParseSingleFieldForms());
  TestClassificationExpectations();
}

// Test that `ParseSingleFieldForms` parses single field IBAN.
TEST_P(FormFieldTest, ParseSingleFieldFormsIban) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(features::kAutofillParseIBANFields);

  // Parse single field IBAN.
  AddTextFormFieldData("", "IBAN", IBAN_VALUE);
  EXPECT_EQ(1, ParseSingleFieldForms());
  TestClassificationExpectations();

  // Don't parse other fields.
  // UNKNOWN_TYPE is used as the expected type, which prevents it from being
  // part of the expectations in `TestClassificationExpectations()`.
  AddTextFormFieldData("", "Address line 1", UNKNOWN_TYPE);
  EXPECT_EQ(1, ParseSingleFieldForms());
  TestClassificationExpectations();
}

struct ParseInAnyOrderTestcase {
  // An nxn matrix, describing that field i is matched by parser j.
  std::vector<std::vector<bool>> field_matches_parser;
  // The expected order in which the n fields are matched, or empty, if the
  // matching is expected to fail.
  std::vector<int> expected_permutation;
};

class ParseInAnyOrderTest
    : public testing::TestWithParam<ParseInAnyOrderTestcase> {};

const ParseInAnyOrderTestcase kParseInAnyOrderTestcases[]{
    // Field i is only matched by parser i -> matched in order.
    {{{true, false, false}, {false, true, false}, {false, false, true}},
     {0, 1, 2}},
    // Opposite order.
    {{{false, true}, {true, false}}, {1, 0}},
    // The second field has to go first, because it is only matched by the first
    // parser.
    {{{true, true}, {true, false}}, {1, 0}},
    // The second parser doesn't match any field, thus no match.
    {{{true, false}, {true, false}}, {}},
    // No field matches.
    {{{false, false}, {false, false}}, {}}};

INSTANTIATE_TEST_SUITE_P(FormFieldTest,
                         ParseInAnyOrderTest,
                         testing::ValuesIn(kParseInAnyOrderTestcases));

TEST_P(ParseInAnyOrderTest, ParseInAnyOrder) {
  auto testcase = GetParam();
  bool expect_success = !testcase.expected_permutation.empty();
  size_t n = testcase.field_matches_parser.size();

  std::vector<std::unique_ptr<AutofillField>> fields;
  // Creates n fields and encodes their ids in `max_length`, as `id_attribute`
  // is a string.
  for (size_t i = 0; i < n; i++) {
    FormFieldData form_field_data;
    form_field_data.max_length = i;
    fields.push_back(std::make_unique<AutofillField>(form_field_data));
  }

  // Checks if `matching_ids` of the `scanner`'s current position is true.
  // This is used to simulate different parsers, as described by
  // `testcase.field_matches_parser`.
  auto Matches = [](AutofillScanner* scanner,
                    const std::vector<bool>& matching_ids) -> bool {
    return matching_ids[scanner->Cursor()->max_length];
  };

  // Construct n parsers from `testcase.field_matches_parser`.
  AutofillScanner scanner(fields);
  std::vector<AutofillField*> matched_fields(n);
  std::vector<std::pair<AutofillField**, base::RepeatingCallback<bool()>>>
      fields_and_parsers;
  for (size_t i = 0; i < n; i++) {
    fields_and_parsers.emplace_back(
        &matched_fields[i],
        base::BindRepeating(Matches, &scanner,
                            testcase.field_matches_parser[i]));
  }

  EXPECT_EQ(FormField::ParseInAnyOrderForTesting(&scanner, fields_and_parsers),
            expect_success);

  if (expect_success) {
    EXPECT_TRUE(scanner.IsEnd());
    ASSERT_EQ(testcase.expected_permutation.size(), n);
    for (size_t i = 0; i < n; i++) {
      EXPECT_EQ(matched_fields[i],
                fields[testcase.expected_permutation[i]].get());
    }
  } else {
    EXPECT_EQ(scanner.CursorPosition(), 0u);
    EXPECT_THAT(matched_fields, ::testing::Each(nullptr));
  }
}

// Today, local heuristics typically classify fields if at least 3 different
// fields get a fillable type assigned. This leads to false positives as in this
// example case. features::kAutofillMin3FieldTypesForLocalHeuristics changes the
// rule to require at least 3 different field *types*.
// Note that "fillable" refers to the field type, not whether a specific field
// is visible and editable by the user.
TEST_P(FormFieldTest, ParseFormRequires3DistinctFieldTypes) {
  AddTextFormFieldData("name_origin", "From:", NAME_FULL);
  AddTextFormFieldData("name_destination", "To:", NAME_FULL);
  AddTextFormFieldData("name_via", "Via...", NAME_FULL);
  AddTextFormFieldData("name_notVia", "Not via...", NAME_FULL);

  // Ensure that the parser does not return anything if
  // features::kAutofillMin3FieldTypesForLocalHeuristics is enabled because it
  // found only 1 field type.
  if (require_min_3_field_types_for_local_heuristics()) {
    EXPECT_EQ(0, ParseFormFields());
  } else {
    EXPECT_EQ(4, ParseFormFields());
    TestClassificationExpectations();
  }

  // Add two more fields and ensure that the parser now returns all fields even
  // in the presence of features::kAutofillMin3FieldTypesForLocalHeuristics.
  AddTextFormFieldData("", "Address line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("", "Address line 2", ADDRESS_HOME_LINE2);
  EXPECT_EQ(6, ParseFormFields());
  TestClassificationExpectations();
}

}  // namespace autofill
