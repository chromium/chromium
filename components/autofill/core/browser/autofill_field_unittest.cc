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
  // This value denotes what should `ComputedType` return as field type.
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
  field.set_heuristic_type(GetActivePatternSource(), test_case.heuristic_type);
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

}  // namespace
}  // namespace autofill
