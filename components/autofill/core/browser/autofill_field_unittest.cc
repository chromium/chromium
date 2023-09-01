// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Parameters for `PrecedenceOverAutocompleteTest`
struct PrecedenceOverAutocompleteParams {
  // These values are used to parameterize feature
  // 'kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete'.
  const features::PrecedenceOverAutocompleteScope heuristic_precedence_scope;
  const features::PrecedenceOverAutocompleteScope server_precedence_scope;
  // These values denote what type the field was viewed by html, server and
  // heuristic prediction.
  const HtmlFieldType html_field_type;
  const ServerFieldType server_type;
  const ServerFieldType heuristic_type;
  // This value denotes what `ComputedType` should return as field type.
  const ServerFieldType expected_result;
};

class PrecedenceOverAutocompleteTest
    : public testing::TestWithParam<PrecedenceOverAutocompleteParams> {
  base::test::ScopedFeatureList scoped_feature_list;

 public:
  PrecedenceOverAutocompleteTest() {
    PrecedenceOverAutocompleteParams test_case = GetParam();
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete,
        {{features::kAutofillHeuristicPrecedenceScopeOverAutocomplete.name,
          features::kAutofillHeuristicPrecedenceScopeOverAutocomplete.GetName(
              test_case.heuristic_precedence_scope)},
         {features::kAutofillServerPrecedenceScopeOverAutocomplete.name,
          features::kAutofillServerPrecedenceScopeOverAutocomplete.GetName(
              test_case.server_precedence_scope)}});
  }
};

// Tests the correctness of the feature giving StreetName or HouseNumber
// predictions, by heuristic or server, precedence over autocomplete.
TEST_P(PrecedenceOverAutocompleteTest, PrecedenceOverAutocompleteParams) {
  PrecedenceOverAutocompleteParams test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.html_field_type, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {test::CreateFieldPrediction(test_case.server_type)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           test_case.heuristic_type);
  EXPECT_EQ(test_case.expected_result, field.ComputedType().GetStorableType());
}

INSTANTIATE_TEST_SUITE_P(
    AutofillFieldTest,
    PrecedenceOverAutocompleteTest,
    testing::Values(
        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kNone,
            .html_field_type = HtmlFieldType::kAddressLine1,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_STREET_NAME,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kNone,
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kNone,
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = NAME_FIRST},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kNone,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = UNKNOWN_TYPE},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kNone,
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = NAME_FIRST,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kNone,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = UNKNOWN_TYPE},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kNone,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = UNKNOWN_TYPE,
            .heuristic_type = ADDRESS_HOME_STREET_NAME,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_LINE2,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = NAME_FIRST},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_STREET_NAME,
            .expected_result = UNKNOWN_TYPE},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_STREET_NAME,
            .expected_result = UNKNOWN_TYPE},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kAddressLine1And2,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_STREET_NAME,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = NAME_FIRST,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_STREET_NAME,
            .expected_result = UNKNOWN_TYPE},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = NAME_FIRST,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kRecognized,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = NAME_FIRST,
            .expected_result = UNKNOWN_TYPE},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_STREET_NAME,
            .expected_result = ADDRESS_HOME_STREET_NAME},

        PrecedenceOverAutocompleteParams{
            .heuristic_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .server_precedence_scope =
                features::PrecedenceOverAutocompleteScope::kSpecified,
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = NAME_FIRST,
            .expected_result = ADDRESS_HOME_STREET_NAME}));

// Tests for type predictions of ac=unrecognized fields when
// `kAutofillPredictionsForAutocompleteUnrecognized` is enabled:
// By default, address Autofill suppresses type predictions for ac=unrecognized
// fields. Consequently, no suggestions are shown for such fields and the fields
// cannot be filled.
// With `kAutofillPredictionsForAutocompleteUnrecognized`, predictions are no
// longer suppressed. Suggestions for ac=unrecognized fields remain suppressed
// and the fields are not filled.
// `AutofillField::ShouldSuppressSuggestionsAndFillingByDefault()` indicates
// that the field should receive special treatment in the suggestion and filling
// logic.
// Every test specifies the predicted type for a field and what the expected
// return value of the aforementioned function is.
struct AutocompleteUnrecognizedTypeTestCase {
  // Either server or heuristic type - this doesn't matter for these tests.
  ServerFieldType predicted_type;
  // If the predicted type should be treated as a server overwrite. Server
  // overwrites already take precedence over ac=unrecognized.
  bool is_server_overwrite = false;
  // Expected value of `ShouldSuppressSuggestionsAndFillingByDefault()`.
  bool expect_should_suppress_suggestions_and_filling;
};

class AutocompleteUnrecognizedTypeTest
    : public testing::TestWithParam<AutocompleteUnrecognizedTypeTestCase> {
 public:
  AutocompleteUnrecognizedTypeTest()
      : feature_(features::kAutofillPredictionsForAutocompleteUnrecognized) {}

 private:
  base::test::ScopedFeatureList feature_;
};

TEST_P(AutocompleteUnrecognizedTypeTest, TypePredictions) {
  // Create a field with ac=unrecognized and the specified predicted type.
  const AutocompleteUnrecognizedTypeTestCase& test = GetParam();
  AutofillField field;
  field.set_server_predictions({test::CreateFieldPrediction(
      test.predicted_type, test.is_server_overwrite)});
  field.SetHtmlType(HtmlFieldType::kUnrecognized, HtmlFieldMode::kNone);

  // Expect that the predicted type wins over ac=unrecognized.
  EXPECT_EQ(field.Type().GetStorableType(), test.predicted_type);
  EXPECT_EQ(field.ShouldSuppressSuggestionsAndFillingByDefault(),
            test.expect_should_suppress_suggestions_and_filling);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillFieldTest,
    AutocompleteUnrecognizedTypeTest,
    testing::Values(
        // Predicted address type: Expect no suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = ADDRESS_HOME_CITY,
            .expect_should_suppress_suggestions_and_filling = true},
        // Server overwrite: Expect suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = ADDRESS_HOME_CITY,
            .is_server_overwrite = true,
            .expect_should_suppress_suggestions_and_filling = false},
        // Credit card prediction: They ignore ac=unrecognized independently of
        // the feature. Thus, expect suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = CREDIT_CARD_NUMBER,
            .expect_should_suppress_suggestions_and_filling = false}));

// Parameters for `AutofillLocalHeuristicsOverridesTest`
struct AutofillLocalHeuristicsOverridesParams {
  // These values denote what type the field was classified as html, server and
  // heuristic prediction.
  const HtmlFieldType html_field_type;
  const ServerFieldType server_type;
  const ServerFieldType heuristic_type;
  // This value denotes what `ComputedType` should return as field type.
  const ServerFieldType expected_result;
};

class AutofillLocalHeuristicsOverridesTest
    : public testing::TestWithParam<AutofillLocalHeuristicsOverridesParams> {
 public:
  AutofillLocalHeuristicsOverridesTest() = default;

 private:
  base::test::ScopedFeatureList feature_{
      features::kAutofillLocalHeuristicsOverrides};
};

// Tests the correctness of local heuristic overrides while computing the
// overall field type.
TEST_P(AutofillLocalHeuristicsOverridesTest,
       AutofillLocalHeuristicsOverridesParams) {
  AutofillLocalHeuristicsOverridesParams test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.html_field_type, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {test::CreateFieldPrediction(test_case.server_type)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           test_case.heuristic_type);
  EXPECT_EQ(test_case.expected_result, field.ComputedType().GetStorableType());
}

INSTANTIATE_TEST_SUITE_P(
    AutofillHeuristicsOverrideTest,
    AutofillLocalHeuristicsOverridesTest,
    testing::Values(
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_CITY,
            .heuristic_type = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_result = ADDRESS_HOME_ADMIN_LEVEL2},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_APT_NUM},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_result = ADDRESS_HOME_ADMIN_LEVEL2},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel2,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_APT_NUM},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel2,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_result = ADDRESS_HOME_DEPENDENT_LOCALITY},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_result = ADDRESS_HOME_DEPENDENT_LOCALITY},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
            .expected_result = ADDRESS_HOME_OVERFLOW_AND_LANDMARK},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_OVERFLOW,
            .expected_result = ADDRESS_HOME_OVERFLOW},
        // Final type is unknown if the html type is not valid.
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = ADDRESS_HOME_CITY,
            .heuristic_type = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_result = UNKNOWN_TYPE},
        // Test non-override behaviour.
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kStreetAddress,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_STREET_ADDRESS,
            .expected_result = ADDRESS_HOME_STREET_ADDRESS},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_CITY,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_CITY}));

}  // namespace
}  // namespace autofill
