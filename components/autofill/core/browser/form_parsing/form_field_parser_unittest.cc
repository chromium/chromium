// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser_test_api.h"
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
    ParsingContext context(fields_, client_country, language,
                           GetActivePatternFile().value(),
                           GetActiveRegexFeatures(), /*log_manager=*/nullptr);
    FormFieldParser::ParseFormFields(context, fields_, field_candidates_map_);
    return field_candidates_map_.size();
  }

  // Like `ParseFormFields()`, but using `ParseSingleFields()` instead.
  int ParseSingleFields() {
    ParsingContext context(fields_, GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value(),
                           GetActiveRegexFeatures(), /*log_manager=*/nullptr);
    FormFieldParser::ParseSingleFields(context, fields_, field_candidates_map_);
    return field_candidates_map_.size();
  }

  int ParseStandaloneCVCFields() {
    ParsingContext context(fields_, GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value(),
                           /*active_features=*/{}, /*log_manager=*/nullptr);
    FormFieldParser::ParseStandaloneCVCFields(context, fields_,
                                              field_candidates_map_);
    return field_candidates_map_.size();
  }

  int ParseStandaloneEmailFields() {
    ParsingContext context(fields_, GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value(),
                           /*active_features=*/{}, /*log_manager=*/nullptr);
    FormFieldParser::ParseStandaloneEmailFields(context, fields_,
                                                field_candidates_map_);
    return field_candidates_map_.size();
  }

  int ParseStandaloneLoyaltyCardFields() {
    ParsingContext context(fields_, GeoIpCountryCode(""), LanguageCode(""),
                           GetActivePatternFile().value(),
                           GetActiveRegexFeatures(), /*log_manager=*/nullptr);
    FormFieldParser::ParseStandaloneLoyaltyCardFields(context, fields_,
                                                      field_candidates_map_);
    return field_candidates_map_.size();
  }

  // FormFieldParserTestBase:
  // This function is unused in these unit tests, because FormFieldParser is not
  // a parser itself, but the infrastructure combining them.
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner& scanner) override {
    return nullptr;
  }
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
  auto field = std::make_unique<AutofillField>();
  SCOPED_TRACE("label = " + base::UTF16ToUTF8(label));
  field->set_label(label);
  for (const auto& pattern : positive_patterns) {
    ParsingContext context(base::span_from_ref(field), GeoIpCountryCode(""),
                           LanguageCode(""), GetActivePatternFile().value(),
                           /*active_features=*/{}, /*log_manager=*/nullptr);
    SCOPED_TRACE("positive_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_TRUE(FormFieldParserTestApi::Match(context, *field, pattern,
                                              {MatchAttribute::kLabel}));
  }
  for (const auto& pattern : negative_patterns) {
    ParsingContext context(base::span_from_ref(field), GeoIpCountryCode(""),
                           LanguageCode(""), GetActivePatternFile().value(),
                           /*active_features=*/{}, /*log_manager=*/nullptr);
    SCOPED_TRACE("negative_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_FALSE(FormFieldParserTestApi::Match(context, *field, pattern,
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
  FormFieldData& field = fields_.back();
  ParsingContext context(fields_, GeoIpCountryCode(""), LanguageCode(""),
                         GetActivePatternFile().value(), /*active_features=*/{},
                         /*log_manager=*/nullptr);
  context.label_overrides[field.global_id()] = u"First Name";
  EXPECT_TRUE(FormFieldParserTestApi::Match(context, field, u"First Name",
                                            {MatchAttribute::kLabel}));
}

// Tests that `ParseSingleFields` is called as part of `ParseFormFields`.
TEST_F(FormFieldParserTest, ParseSingleFieldsInsideParseFormField) {
  AddTextFormFieldData("", "Phone", PHONE_HOME_CITY_AND_NUMBER);
  AddTextFormFieldData("", "Email", EMAIL_ADDRESS);
  AddTextFormFieldData("", "Promo code", MERCHANT_PROMO_CODE);

  // `ParseSingleFields` should detect the promo code.
  EXPECT_EQ(3, ParseFormFields());
  TestClassificationExpectations();
}

// Test that `ParseSingleFields` parses single field promo codes.
TEST_F(FormFieldParserTest, ParseFormFieldsForSingleFieldPromoCode) {
  // Parse single field promo code.
  AddTextFormFieldData("", "Promo code", MERCHANT_PROMO_CODE);
  EXPECT_EQ(1, ParseSingleFields());
  TestClassificationExpectations();

  // Don't parse other fields.
  // UNKNOWN_TYPE is used as the expected type, which prevents it from being
  // part of the expectations in `TestClassificationExpectations()`.
  AddTextFormFieldData("", "Address line 1", UNKNOWN_TYPE);
  EXPECT_EQ(1, ParseSingleFields());
  TestClassificationExpectations();
}

// Test that `ParseSingleFields` parses single field IBAN.
TEST_F(FormFieldParserTest, ParseSingleFieldsIban) {
  // Parse single field IBAN.
  AddTextFormFieldData("", "IBAN", IBAN_VALUE);
  EXPECT_EQ(1, ParseSingleFields());
  TestClassificationExpectations();

  // Don't parse other fields.
  // UNKNOWN_TYPE is used as the expected type, which prevents it from being
  // part of the expectations in `TestClassificationExpectations()`.
  AddTextFormFieldData("", "Address line 1", UNKNOWN_TYPE);
  EXPECT_EQ(1, ParseSingleFields());
  TestClassificationExpectations();
}

// Tests that loyalty cards are parsed as part of `ParseFormFields`.
TEST_F(FormFieldParserTest, ParseFormFieldsFieldsLoyaltyCard) {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnableLoyaltyCardsFilling};
  AddTextFormFieldData("", "Email", EMAIL_ADDRESS);
  AddTextFormFieldData("", "Frequent Flyer", LOYALTY_MEMBERSHIP_ID);

  // `ParseSingleFields` should detect the loyalty card field.
  EXPECT_EQ(2, ParseFormFields());
  TestClassificationExpectations();
}

// Test that `ParseSingleFields` parses a single loyalty card field.
// LOYALTY_MEMBERSHIP_ID is allowlisted to be produced by the field
// classification even if the form does not have >= 3 recognized field types.
TEST_F(FormFieldParserTest, ParseStandaloneLoyaltyCardFields) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableLoyaltyCardsFilling,
       features::kAutofillEnableEmailOrLoyaltyCardsFilling},
      {});

  // Parse single field loyalty card.
  AddTextFormFieldData("", "frequent-flyer", LOYALTY_MEMBERSHIP_ID);
  EXPECT_EQ(1, ParseStandaloneLoyaltyCardFields());
  TestClassificationExpectations();

  // Don't parse other fields.
  // UNKNOWN_TYPE is used as the expected type, which prevents it from being
  // part of the expectations in `TestClassificationExpectations()`.
  AddTextFormFieldData("", "Address line 1", UNKNOWN_TYPE);
  EXPECT_EQ(1, ParseStandaloneLoyaltyCardFields());
  TestClassificationExpectations();
}

// Tests that email or loyalty cards fields are parsed as
// `EMAIL_OR_LOYALTY_MEMBERSHIP_ID`.
TEST_F(FormFieldParserTest, ParseFormFieldsFieldsEmailOrLoyaltyCard) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableLoyaltyCardsFilling,
       features::kAutofillEnableEmailOrLoyaltyCardsFilling},
      {});
  AddTextFormFieldData("", "Email Or Loyalty Card",
                       EMAIL_OR_LOYALTY_MEMBERSHIP_ID);
  AddTextFormFieldData("", "Password", UNKNOWN_TYPE);

  // `ParseFormFields` should detect the email or loyalty card field.
  EXPECT_EQ(1, ParseFormFields());
  TestClassificationExpectations();
}

// Test that `ParseStandaloneCvcField` parses standalone CVC fields.
TEST_F(FormFieldParserTest, ParseStandaloneCVCFields) {
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

  std::vector<FormFieldData> fields;
  // Creates n fields and encodes their ids in `max_length`, as `id_attribute`
  // is a string.
  for (size_t i = 0; i < n; i++) {
    FormFieldData form_field_data;
    form_field_data.set_max_length(i);
    fields.push_back(form_field_data);
  }

  // Checks if `matching_ids` of the `scanner`'s current position is true.
  // This is used to simulate different parsers, as described by
  // `testcase.field_matches_parser`.
  auto Matches = [](AutofillScanner& scanner,
                    const std::vector<bool>& matching_ids) -> bool {
    return matching_ids[scanner.Cursor().max_length()];
  };

  // Construct `n` parsers from `testcase.field_matches_parser`.
  // Since base::FunctionRef is non-owning, we need to define at least `n`
  // lambdas by hand.
  AutofillScanner scanner(fields,
                          [](const FormFieldData& field) { return true; });
  CHECK_LE(n, 3u) << "If a test case has size > 3, add a `callbackN` variable "
                     "below and add it to `callbacks`";
  auto callback0 = [&]() {
    return Matches(scanner, testcase.field_matches_parser[0]);
  };
  auto callback1 = [&]() {
    return Matches(scanner, testcase.field_matches_parser[1]);
  };
  auto callback2 = [&]() {
    return Matches(scanner, testcase.field_matches_parser[2]);
  };
  auto callbacks = std::to_array<base::FunctionRef<bool()>>(
      {callback0, callback1, callback2});
  std::vector<raw_ptr<const FormFieldData>> matched_fields(n);
  std::vector<
      std::pair<raw_ptr<const FormFieldData>*, base::FunctionRef<bool()>>>
      fields_and_parsers;
  for (size_t i = 0; i < n; i++) {
    fields_and_parsers.emplace_back(&matched_fields[i], callbacks[i]);
  }

  EXPECT_EQ(
      FormFieldParserTestApi::ParseInAnyOrder(scanner, fields_and_parsers),
      expect_success);

  if (expect_success) {
    EXPECT_TRUE(scanner.IsEnd());
    ASSERT_EQ(testcase.expected_permutation.size(), n);
    for (size_t i = 0; i < n; i++) {
      EXPECT_EQ(matched_fields[i], &fields[testcase.expected_permutation[i]]);
    }
  } else {
    EXPECT_EQ(scanner.GetOffset(), 0u);
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

// Tests that:
// - High quality label matches are prioritized over low quality label matches.
// - Names matches are considered equally important as high quality label
//   matches and ties are broken by parser level scores.
TEST_F(FormFieldParserTest, LabelPrioritization) {
  base::test::ScopedFeatureList feature{
      features::kAutofillBetterLocalHeuristicPlaceholderSupport};

  // - High quality name-type label.
  // - Low quality address-type placeholder.
  //   => High quality name-type wins.
  AddFormFieldData(FormControlType::kInputText, /*name=*/"",
                   /*label=*/"Full name", /*placeholder=*/"Street address",
                   /*max_length=*/0, NAME_FULL);
  fields_.back().set_label_source(FormFieldData::LabelSource::kForId);

  // - Low quality name-type label.
  // - High quality address-type placeholder.
  //   => Placeholder wins.
  // (The expected type is address line 2, as the address parser qualifies the
  //  first field as address line 1 internally, but this is overruled by the
  //  higher priority name type. In practice rationalisation would fix the type)
  AddFormFieldData(FormControlType::kInputText, /*name=*/"",
                   /*label=*/"Full name", /*placeholder=*/"Street address",
                   /*max_length=*/0, ADDRESS_HOME_LINE2);
  fields_.back().set_label_source(FormFieldData::LabelSource::kDivTable);

  // - High quality name-type label.
  // - Low quality address-type placeholder.
  // - Email-type name.
  //   => Name wins because `kBaseEmailParserScore` > `kBaseNameParserScore`.
  AddFormFieldData(FormControlType::kInputText, /*name=*/"email",
                   /*label=*/"Full name", /*placeholder=*/"Street address",
                   /*max_length=*/0, EMAIL_ADDRESS);
  fields_.back().set_label_source(FormFieldData::LabelSource::kForId);

  // - Low quality name-type label.
  // - High quality address-type placeholder.
  // - Email-type name.
  //   => Name wins because `kBaseEmailParserScore` > `kBaseAddressParserScore`.
  AddFormFieldData(FormControlType::kInputText, /*name=*/"email",
                   /*label=*/"Full name", /*placeholder=*/"Street address",
                   /*max_length=*/0, EMAIL_ADDRESS);
  fields_.back().set_label_source(FormFieldData::LabelSource::kDivTable);

  EXPECT_EQ(4, ParseFormFields());
  TestClassificationExpectations();
}

}  // namespace autofill
