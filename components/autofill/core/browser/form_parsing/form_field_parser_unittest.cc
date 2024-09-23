// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class FormFieldParserTest : public FormFieldParserTestBase,
                            public ::testing::Test {
 public:
  FormFieldParserTest() = default;
  FormFieldParserTest(const FormFieldParserTest&) = delete;
  FormFieldParserTest& operator=(const FormFieldParserTest&) = delete;

 protected:
  // Parses all added fields using `ParseFormFields`.
  // Returns the number of fields parsed.
  int ParseFormFields(GeoIpCountryCode client_country = GeoIpCountryCode(""),
                      LanguageCode language = LanguageCode("")) {
    ParsingContext context(client_country, language,
                           GetActivePatternFile().value());
    FormFieldParser::ParseFormFields(context, fields_,
                                     /*is_form_tag=*/true,
                                     field_candidates_map_);
    return field_candidates_map_.size();
  }

  // Like `ParseFormFields()`, but using `ParseSingleFieldForms()` instead.
  int ParseSingleFieldForms() {
    ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value());
    FormFieldParser::ParseSingleFieldForms(context, fields_,
                                           field_candidates_map_);
    return field_candidates_map_.size();
  }

  int ParseStandaloneCVCFields() {
    ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value());
    FormFieldParser::ParseStandaloneCVCFields(context, fields_,
                                              field_candidates_map_);
    return field_candidates_map_.size();
  }

  int ParseStandaloneEmailFields() {
    ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value());
    FormFieldParser::ParseStandaloneEmailFields(context, fields_,
                                                field_candidates_map_);
    return field_candidates_map_.size();
  }

  // FormFieldParserTestBase:
  // This function is unused in these unit tests, because FormFieldParser is not
  // a parser itself, but the infrastructure combining them.
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

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

INSTANTIATE_TEST_SUITE_P(FormFieldParserTest,
                         MatchTest,
                         testing::ValuesIn(kMatchTestCases));

TEST_P(MatchTest, Match) {
  const auto& [label, positive_patterns, negative_patterns] = GetParam();
  AutofillField field;
  SCOPED_TRACE("label = " + base::UTF16ToUTF8(label));
  field.set_label(label);
  field.set_parseable_label(label);
  for (const auto& pattern : positive_patterns) {
    ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value());
    SCOPED_TRACE("positive_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_TRUE(FormFieldParser::MatchForTesting(context, &field, pattern,
                                                 {MatchAttribute::kLabel}));
  }
  for (const auto& pattern : negative_patterns) {
    ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value());
    SCOPED_TRACE("negative_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_FALSE(FormFieldParser::MatchForTesting(context, &field, pattern,
                                                  {MatchAttribute::kLabel}));
  }
}

// Test that we ignore checkable elements.
TEST_F(FormFieldParserTest, ParseFormFieldsIgnoreCheckableElements) {
  AddFormFieldData(FormControlType::kInputCheckbox, "", "Is PO Box",
                   UNKNOWN_TYPE);
  // Add 3 dummy fields to reach kMinRequiredFieldsForHeuristics = 3.
  AddTextFormFieldData("", "Address line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("", "Address line 2", ADDRESS_HOME_LINE2);
  AddTextFormFieldData("", "Address line 3", ADDRESS_HOME_LINE3);
  EXPECT_EQ(3, ParseFormFields());
  TestClassificationExpectations();
}

// Test that the minimum number of required fields for the heuristics considers
// whether a field is actually fillable.
TEST_F(FormFieldParserTest, ParseFormFieldsEnforceMinFillableFields) {
  AddTextFormFieldData("", "Address line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("", "Address line 2", ADDRESS_HOME_LINE2);
  AddTextFormFieldData("", "Search", SEARCH_TERM);
  // We don't parse the form because search fields are not fillable (therefore,
  // the form has only 2 fillable fields).
  EXPECT_EQ(0, ParseFormFields());
}

// Test that the parseable label is used when the feature is enabled.
TEST_F(FormFieldParserTest, TestParseableLabels) {
  AddTextFormFieldData("", "not a parseable label", UNKNOWN_TYPE);
  AutofillField* autofill_field = fields_.back().get();
  autofill_field->set_parseable_label(u"First Name");

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value());
    EXPECT_TRUE(FormFieldParser::MatchForTesting(
        context, autofill_field, u"First Name", {MatchAttribute::kLabel}));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value());
    EXPECT_FALSE(FormFieldParser::MatchForTesting(
        context, autofill_field, u"First Name", {MatchAttribute::kLabel}));
  }
}

// Tests that `ParseSingleFieldForms` is called as part of `ParseFormFields`.
TEST_F(FormFieldParserTest, ParseSingleFieldFormsInsideParseFormField) {
  AddTextFormFieldData("", "Phone", PHONE_HOME_CITY_AND_NUMBER);
  AddTextFormFieldData("", "Email", EMAIL_ADDRESS);
  AddTextFormFieldData("", "Promo code", MERCHANT_PROMO_CODE);

  // `ParseSingleFieldForms` should detect the promo code.
  EXPECT_EQ(3, ParseFormFields());
  TestClassificationExpectations();
}

// Test that `ParseSingleFieldForms` parses single field promo codes.
TEST_F(FormFieldParserTest, ParseFormFieldsForSingleFieldPromoCode) {
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
TEST_F(FormFieldParserTest, ParseSingleFieldFormsIban) {
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

// Test that `ParseStandaloneCvcField` parses standalone CVC fields.
TEST_F(FormFieldParserTest, ParseStandaloneCVCFields) {
  base::test::ScopedFeatureList scoped_feature(
      features::kAutofillParseVcnCardOnFileStandaloneCvcFields);

  AddTextFormFieldData("", "CVC", CREDIT_CARD_STANDALONE_VERIFICATION_CODE);
  EXPECT_EQ(1, ParseStandaloneCVCFields());
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

INSTANTIATE_TEST_SUITE_P(FormFieldParserTest,
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
    form_field_data.set_max_length(i);
    fields.push_back(std::make_unique<AutofillField>(form_field_data));
  }

  // Checks if `matching_ids` of the `scanner`'s current position is true.
  // This is used to simulate different parsers, as described by
  // `testcase.field_matches_parser`.
  auto Matches = [](AutofillScanner* scanner,
                    const std::vector<bool>& matching_ids) -> bool {
    return matching_ids[scanner->Cursor()->max_length()];
  };

  // Construct n parsers from `testcase.field_matches_parser`.
  AutofillScanner scanner(fields);
  std::vector<raw_ptr<AutofillField>> matched_fields(n);
  std::vector<
      std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
      fields_and_parsers;
  for (size_t i = 0; i < n; i++) {
    fields_and_parsers.emplace_back(
        &matched_fields[i],
        base::BindRepeating(Matches, &scanner,
                            testcase.field_matches_parser[i]));
  }

  EXPECT_EQ(
      FormFieldParser::ParseInAnyOrderForTesting(&scanner, fields_and_parsers),
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
TEST_F(FormFieldParserTest, ParseFormRequires3DistinctFieldTypes) {
  AddTextFormFieldData("name_origin", "From:", NAME_FULL);
  AddTextFormFieldData("name_destination", "To:", NAME_FULL);
  AddTextFormFieldData("name_via", "Via...", NAME_FULL);
  AddTextFormFieldData("name_notVia", "Not via...", NAME_FULL);

  // Ensure that the parser does not return anything because it found only 1
  // field type.
  EXPECT_EQ(0, ParseFormFields());

  // Add two more fields and ensure that the parser now returns all fields even
  // in the presence of features::kAutofillMin3FieldTypesForLocalHeuristics.
  AddTextFormFieldData("", "Address line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("", "Address line 2", ADDRESS_HOME_LINE2);
  EXPECT_EQ(6, ParseFormFields());
  TestClassificationExpectations();
}

TEST_F(FormFieldParserTest, ParseStandaloneZipDisabledForUS) {
  AddTextFormFieldData("zip", "ZIP", ADDRESS_HOME_ZIP);
  EXPECT_EQ(0, ParseFormFields(GeoIpCountryCode("US")));
}

TEST_F(FormFieldParserTest, ParseStandaloneZipEnabledForBR) {
  AddTextFormFieldData("cep", "CEP", ADDRESS_HOME_ZIP);
  EXPECT_EQ(1, ParseFormFields(GeoIpCountryCode("BR")));
  TestClassificationExpectations();
}

TEST_F(FormFieldParserTest, ParseStandaloneEmail) {
  AddTextFormFieldData("email", "email", EMAIL_ADDRESS);
  AddTextFormFieldData("unknown", "Horseradish", UNKNOWN_TYPE);
  EXPECT_EQ(1, ParseStandaloneEmailFields());
  TestClassificationExpectations();
}

TEST_F(FormFieldParserTest, ParseStandaloneEmailWithNoEmailFields) {
  AddTextFormFieldData("unknown", "Horseradish", UNKNOWN_TYPE);
  EXPECT_EQ(0, ParseStandaloneEmailFields());
  TestClassificationExpectations();
}

// Tests that an email field is recognized even though it matches the pattern
// nombre.*dirección, which is used to detect address name/type patterns.
TEST_F(FormFieldParserTest, ParseStandaloneEmailSimilarToAddressName) {
  AddTextFormFieldData("-",
                       "nombre de usuario o dirección de correo electrónico",
                       EMAIL_ADDRESS);
  AddTextFormFieldData("city", "City", ADDRESS_HOME_CITY);
  AddTextFormFieldData("state", "State", ADDRESS_HOME_STATE);
  AddTextFormFieldData("zip", "Zip", ADDRESS_HOME_ZIP);
  EXPECT_EQ(4, ParseFormFields(GeoIpCountryCode("BR"), LanguageCode("es")));
  TestClassificationExpectations();
}

}  // namespace autofill
