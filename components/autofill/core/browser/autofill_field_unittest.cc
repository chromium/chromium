// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include <optional>
#include <variant>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/field_prediction_test_matchers.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::autofill::test::CreateFieldPrediction;
using ::autofill::test::EqualsPrediction;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

constexpr FieldTypeSet kMLSupportedTypesForTesting = {
    UNKNOWN_TYPE,       NAME_FIRST,
    NAME_LAST,          EMAIL_ADDRESS,
    NAME_FULL,          PHONE_HOME_NUMBER,
    ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_ADDRESS,
    ADDRESS_HOME_CITY};

class AutofillFieldTest : public testing::Test {
 public:
  AutofillFieldTest() = default;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Tests that if both autocomplete attributes and server agree it's a phone
// field, always use server predicted type. If they disagree with autocomplete
// says it's a phone field, always use autocomplete attribute.
TEST_F(AutofillFieldTest, Type_ServerPredictionOfCityAndNumber_OverrideHtml) {
  AutofillField field;

  field.SetHtmlType(HtmlFieldType::kTel, HtmlFieldMode::kNone);

  field.set_server_predictions(
      {CreateFieldPrediction(PHONE_HOME_CITY_AND_NUMBER)});
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_EQ(field.PredictionSource(),
            AutofillPredictionSource::kServerCrowdsourcing);

  // Overrides to another number format.
  field.set_server_predictions({CreateFieldPrediction(PHONE_HOME_NUMBER)});
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(PHONE_HOME_NUMBER));
  EXPECT_EQ(field.PredictionSource(),
            AutofillPredictionSource::kServerCrowdsourcing);

  // Overrides autocomplete=tel-national too.
  field.SetHtmlType(HtmlFieldType::kTelNational, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {CreateFieldPrediction(PHONE_HOME_WHOLE_NUMBER)});
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(field.PredictionSource(),
            AutofillPredictionSource::kServerCrowdsourcing);

  // If autocomplete=tel-national but server says it's not a phone field,
  // do not override.
  field.SetHtmlType(HtmlFieldType::kTelNational, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {CreateFieldPrediction(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)});
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_EQ(field.PredictionSource(), AutofillPredictionSource::kAutocomplete);

  // If html type not specified, we still use server prediction.
  field.SetHtmlType(HtmlFieldType::kUnspecified, HtmlFieldMode::kNone);
  field.set_server_predictions(
      {CreateFieldPrediction(PHONE_HOME_CITY_AND_NUMBER)});
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_EQ(field.PredictionSource(),
            AutofillPredictionSource::kServerCrowdsourcing);
}

TEST_F(AutofillFieldTest, IsFieldFillable) {
  AutofillField field;
  ASSERT_THAT(field.Type().GetTypes(), ElementsAre(UNKNOWN_TYPE));

  // Type is unknown.
  EXPECT_FALSE(field.IsFieldFillable());

  // Only heuristic type is set.
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);
  EXPECT_TRUE(field.IsFieldFillable());

  // Only server type is set.
  field.set_heuristic_type(GetActiveHeuristicSource(), UNKNOWN_TYPE);
  field.set_server_predictions({CreateFieldPrediction(NAME_LAST)});
  EXPECT_TRUE(field.IsFieldFillable());

  // Both types set.
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);
  field.set_server_predictions({CreateFieldPrediction(NAME_LAST)});
  EXPECT_TRUE(field.IsFieldFillable());

  // Field has autocomplete="off" set. Since autofill was able to make a
  // prediction, it is still considered a fillable field.
  field.set_should_autocomplete(false);
  EXPECT_TRUE(field.IsFieldFillable());
}

TEST_F(AutofillFieldTest, LoyaltyCardPredictionsIgnoredIfFlagIsDisabled) {
  base::test::ScopedFeatureList feature_;
  feature_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          features::kAutofillEnableLoyaltyCardsFilling,
          features::kAutofillEnableEmailOrLoyaltyCardsFilling});

  AutofillField field;
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(UNKNOWN_TYPE));

  // Both types set.
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);
  field.set_server_predictions({CreateFieldPrediction(LOYALTY_MEMBERSHIP_ID)});

  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(NAME_FIRST));
}

TEST_F(AutofillFieldTest, NoPredictions) {
  AutofillField field;
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(UNKNOWN_TYPE));
  EXPECT_EQ(field.PredictionSource(), std::nullopt);
}

// There are two ways to map a HtmlFieldType `t` to a FieldTypeGroup:
// - `GroupTypeOfHtmlFieldType(t)`
// - `GroupTypeOfFieldType(HtmlFieldTypeToBestCorrespondingFieldType(t))`
// This test validates that they're mostly equivalent.
// TODO(crbug.com/432827625): Make them fully equivalent.
TEST_F(AutofillFieldTest, GroupsOfHtmlTypes) {
  using enum HtmlFieldType;
  static constexpr DenseSet<HtmlFieldType> kInconsistent = {
      kTransactionAmount, kTransactionCurrency};
  for (HtmlFieldType t : HtmlFieldTypeSet::all()) {
    SCOPED_TRACE(testing::Message()
                 << "HtmlFieldType: " << FieldTypeToStringView(t));
    if (kInconsistent.contains(t)) {
      continue;
    }
    FieldTypeGroup g1 = GroupTypeOfHtmlFieldType(t);
    FieldTypeGroup g2 =
        GroupTypeOfFieldType(HtmlFieldTypeToBestCorrespondingFieldType(t));
    EXPECT_EQ(g1, g2);
  }
}

// Tests that if multiple server types are set, AutofillField::Type().GetTypes()
// contains the first and perhaps also the subsequent ones.
TEST_F(AutofillFieldTest, UnionTypesFromServerTypes) {
  // Returns the AutofillType::GetTypes() computed by AutofillField from the
  // given server types.
  auto f = [](auto... server_types) {
    AutofillField field;
    field.set_server_predictions({CreateFieldPrediction(server_types)...});
    return field.Type().GetTypes();
  };

  constexpr FieldType kInvalidFieldType =
      static_cast<FieldType>(15);  // nocheck
  ASSERT_EQ(ToSafeFieldType(kInvalidFieldType, NO_SERVER_DATA), NO_SERVER_DATA);

  EXPECT_THAT(f(), ElementsAre(UNKNOWN_TYPE));

  // NO_SERVER_DATA is mapped to UNKNOWN_TYPE in
  // AutofillField::GetComputedPredictionResult();
  EXPECT_THAT(f(NO_SERVER_DATA), ElementsAre(UNKNOWN_TYPE));
  EXPECT_THAT(f(kInvalidFieldType), ElementsAre(UNKNOWN_TYPE));
  EXPECT_THAT(f(UNKNOWN_TYPE), ElementsAre(UNKNOWN_TYPE));

  EXPECT_THAT(f(ADDRESS_HOME_ZIP), ElementsAre(ADDRESS_HOME_ZIP));
  EXPECT_THAT(f(PASSWORD), ElementsAre(PASSWORD));
  EXPECT_THAT(f(NAME_FIRST, USERNAME), ElementsAre(NAME_FIRST));
  EXPECT_THAT(f(USERNAME, NAME_FIRST), ElementsAre(USERNAME));

  base::test::ScopedFeatureList feature_list(
      features::kAutofillAiWithDataSchema);

  // The Autofill AI predictions are part of the overall type.
  EXPECT_THAT(
      f(ADDRESS_HOME_COUNTRY, PASSPORT_ISSUING_COUNTRY),
      UnorderedElementsAre(ADDRESS_HOME_COUNTRY, PASSPORT_ISSUING_COUNTRY));
  EXPECT_THAT(f(PASSPORT_ISSUING_COUNTRY, ADDRESS_HOME_COUNTRY),
              UnorderedElementsAre(PASSPORT_ISSUING_COUNTRY));
  // Multiple Autofill AI predictions may coexist.
  EXPECT_THAT(
      f(ADDRESS_HOME_COUNTRY, DRIVERS_LICENSE_REGION, VEHICLE_PLATE_STATE),
      UnorderedElementsAre(ADDRESS_HOME_COUNTRY, DRIVERS_LICENSE_REGION,
                           VEHICLE_PLATE_STATE));
  // Conflict resolution: when there are multiple predictions from the same
  // entities, we take the longest prefix that satisfies the AutofillType
  // constraints.
  EXPECT_THAT(f(NAME_FULL, DRIVERS_LICENSE_NUMBER),
              UnorderedElementsAre(NAME_FULL));
  EXPECT_THAT(
      f(ADDRESS_HOME_COUNTRY, DRIVERS_LICENSE_NUMBER, DRIVERS_LICENSE_REGION,
        VEHICLE_LICENSE_PLATE),
      UnorderedElementsAre(ADDRESS_HOME_COUNTRY, DRIVERS_LICENSE_NUMBER));
  EXPECT_THAT(
      f(ADDRESS_HOME_COUNTRY, DRIVERS_LICENSE_NUMBER, DRIVERS_LICENSE_REGION,
        VEHICLE_LICENSE_PLATE, VEHICLE_PLATE_STATE),
      UnorderedElementsAre(ADDRESS_HOME_COUNTRY, DRIVERS_LICENSE_NUMBER));
  EXPECT_THAT(
      f(ADDRESS_HOME_COUNTRY, ADDRESS_HOME_STATE, DRIVERS_LICENSE_NUMBER,
        DRIVERS_LICENSE_REGION, VEHICLE_LICENSE_PLATE, VEHICLE_PLATE_STATE),
      UnorderedElementsAre(ADDRESS_HOME_COUNTRY, DRIVERS_LICENSE_NUMBER));
}

// Tests that if a heuristic type is set, additional server types may influence
// the union type.
TEST_F(AutofillFieldTest, UnionTypesFromHeuristicAndServerTypes) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillAiWithDataSchema);

  // Returns the AutofillType::GetTypes() computed by AutofillField from the
  // given heuristic types and server types.
  auto f = [](FieldType heuristic_type, auto... server_types) {
    AutofillField field;
    field.set_heuristic_type(HeuristicSource::kRegexes, heuristic_type);
    field.set_server_predictions({CreateFieldPrediction(server_types)...});
    return field.Type().GetTypes();
  };

  EXPECT_THAT(f(UNKNOWN_TYPE), ElementsAre(UNKNOWN_TYPE));

  EXPECT_THAT(f(ADDRESS_HOME_ZIP), ElementsAre(ADDRESS_HOME_ZIP));
  EXPECT_THAT(f(PASSWORD), ElementsAre(PASSWORD));

  // The heuristic prediction loses against the server type.
  EXPECT_THAT(f(NAME_FIRST, USERNAME), ElementsAre(USERNAME));
  EXPECT_THAT(f(NAME_FIRST, USERNAME, PASSPORT_NUMBER),
              ElementsAre(USERNAME, PASSPORT_NUMBER));

  // The heuristic type CC_NAME_FULL trumps NAME_FULL.
  EXPECT_THAT(f(CREDIT_CARD_NAME_FULL, NAME_FULL),
              ElementsAre(CREDIT_CARD_NAME_FULL));

  // The heuristic type CC_NAME_FULL trumps NAME_FULL.
  // `PASSPORT_NUMBER` is added to the union type.
  EXPECT_THAT(f(CREDIT_CARD_NAME_FULL, NAME_FULL, PASSPORT_NUMBER),
              ElementsAre(CREDIT_CARD_NAME_FULL, PASSPORT_NUMBER));

  // `PASSPORT_NUMBER` trumps the heuristic prediction.
  EXPECT_THAT(f(CREDIT_CARD_NAME_FULL, PASSPORT_NUMBER, NAME_FULL),
              ElementsAre(PASSPORT_NUMBER));
}

// Tests that if an HTML type is set, additional server types may influence the
// union type.
TEST_F(AutofillFieldTest, UnionTypesFromHtmlAndServerTypes) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillAiWithDataSchema);

  // Returns the AutofillType computed by AutofillField from the given HTML and
  // server types.
  auto f = [](HtmlFieldType html_type, auto... server_types) {
    AutofillField field;
    field.SetHtmlType(html_type, HtmlFieldMode::kNone);
    field.set_server_predictions({CreateFieldPrediction(server_types)...});
    return field.Type();
  };

  auto is_type = [](FieldTypeSet types, bool is_country_code = false) {
    return AllOf(Property(&AutofillType::GetTypes, types),
                 Property(&AutofillType::is_country_code, is_country_code));
  };

  using enum HtmlFieldType;

  EXPECT_THAT(f(kUnrecognized, ADDRESS_HOME_ZIP), is_type({ADDRESS_HOME_ZIP}));

  EXPECT_THAT(f(kCountryName), is_type({ADDRESS_HOME_COUNTRY}, false));
  EXPECT_THAT(f(kCountryCode), is_type({ADDRESS_HOME_COUNTRY}, true));

  // `PASSPORT_NUMBER` is added to the union type.
  EXPECT_THAT(f(kCountryName, PASSPORT_NUMBER),
              is_type({ADDRESS_HOME_COUNTRY, PASSPORT_NUMBER}, false));
  EXPECT_THAT(f(kCountryCode, PASSPORT_NUMBER),
              is_type({ADDRESS_HOME_COUNTRY, PASSPORT_NUMBER}, true));

  // `ADDRESS_HOME_ZIP` is not added to the union type because it is not an
  // Autofill AI type.
  EXPECT_THAT(f(kCountryName, ADDRESS_HOME_ZIP),
              is_type({ADDRESS_HOME_COUNTRY}, false));
  EXPECT_THAT(f(kCountryCode, ADDRESS_HOME_ZIP),
              is_type({ADDRESS_HOME_COUNTRY}, true));

  // `PASSPORT_NUMBER` is added to the union type.
  // `ADDRESS_HOME_ZIP` is ignored because it is not an Autofill AI type.
  EXPECT_THAT(f(kCountryName, PASSPORT_NUMBER, ADDRESS_HOME_ZIP),
              is_type({ADDRESS_HOME_COUNTRY, PASSPORT_NUMBER}, false));
  EXPECT_THAT(f(kCountryCode, PASSPORT_NUMBER, ADDRESS_HOME_ZIP),
              is_type({ADDRESS_HOME_COUNTRY, PASSPORT_NUMBER}, true));

  // `PASSPORT_NUMBER` is added to the union type.
  // `ADDRESS_HOME_ZIP` is ignored because it is not an Autofill AI type.
  EXPECT_THAT(f(kCountryName, ADDRESS_HOME_ZIP, PASSPORT_NUMBER),
              is_type({ADDRESS_HOME_COUNTRY, PASSPORT_NUMBER}, false));
  EXPECT_THAT(f(kCountryCode, ADDRESS_HOME_ZIP, PASSPORT_NUMBER),
              is_type({ADDRESS_HOME_COUNTRY, PASSPORT_NUMBER}, true));
}

constexpr HeuristicSource kRegexSource = HeuristicSource::kRegexes;
constexpr HeuristicSource kMlSource = HeuristicSource::kAutofillMachineLearning;

class AutofillFieldTest_MLPredictions : public AutofillFieldTest {
 public:
  void SetUp() override {
    AutofillFieldTest::SetUp();
    base::FieldTrialParams feature_params;
    feature_params["model_active"] = "true";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillModelPredictions, feature_params);
    field().set_ml_supported_types(kMLSupportedTypesForTesting);
  }

  AutofillField& field() { return field_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  AutofillField field_;
};

// Test that the model prediction is used if set as the active heuristic source.
TEST_F(AutofillFieldTest_MLPredictions, PredictionsUsed) {
  field().set_heuristic_type(kMlSource, ADDRESS_HOME_STREET_ADDRESS);
  field().set_heuristic_type(kRegexSource, ADDRESS_HOME_LINE1);
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS, field().heuristic_type());
}

// Test that the regex prediction is used if the model returned NO_SERVER_DATA.
TEST_F(AutofillFieldTest_MLPredictions, FallbackToRegex_OnNoServerData) {
  field().set_heuristic_type(kMlSource, NO_SERVER_DATA);
  field().set_heuristic_type(kRegexSource, ADDRESS_HOME_LINE1);
  EXPECT_EQ(ADDRESS_HOME_LINE1, field().heuristic_type());
}

// Test that the regex prediction is used if the regex prediction is a type
// unsupported by the model.
TEST_F(AutofillFieldTest_MLPredictions, FallbackToRegex_OnUnsupportedType) {
  field().set_heuristic_type(kMlSource, NAME_FIRST);
  field().set_heuristic_type(kRegexSource, IBAN_VALUE);
  EXPECT_EQ(IBAN_VALUE, field().heuristic_type());

  field().set_heuristic_type(kMlSource, NAME_FIRST);
  field().set_heuristic_type(kRegexSource, PASSPORT_NUMBER);
  EXPECT_EQ(PASSPORT_NUMBER, field().heuristic_type());
}

class AutofillFieldWithAutofillAiTest : public base::test::WithFeatureOverride,
                                        public AutofillFieldTest {
 public:
  AutofillFieldWithAutofillAiTest()
      : base::test::WithFeatureOverride(features::kAutofillAiWithDataSchema) {}
};

// Tests that server prediction with SOURCE_AUTOFILL_AI are only added if
// `features::kAutofillAiWithDataSchema` is enabled.
TEST_P(AutofillFieldWithAutofillAiTest, SetAutofillAiPredictions) {
  AutofillField field;

  const FieldPrediction crowdsourcing_prediction = CreateFieldPrediction(
      NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
  const FieldPrediction ai_prediction =
      CreateFieldPrediction(NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_AI);
  field.set_server_predictions({crowdsourcing_prediction, ai_prediction});

  if (IsParamFeatureEnabled()) {
    EXPECT_THAT(field.server_predictions(),
                ElementsAre(EqualsPrediction(crowdsourcing_prediction),
                            EqualsPrediction(ai_prediction)));
  } else {
    EXPECT_THAT(field.server_predictions(),
                ElementsAre(EqualsPrediction(crowdsourcing_prediction)));
  }
}

// Tests that server prediction with SOURCE_AUTOFILL_AI_CROWDSOURCING are only
// added if `features::kAutofillAiWithDataSchema` is enabled.
TEST_P(AutofillFieldWithAutofillAiTest,
       SetAutofillAiPredictionsFromCrowdsourcing) {
  AutofillField field;

  const FieldPrediction crowdsourcing_prediction = CreateFieldPrediction(
      NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
  const FieldPrediction ai_prediction = CreateFieldPrediction(
      NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_AI_CROWDSOURCING);
  field.set_server_predictions({crowdsourcing_prediction, ai_prediction});

  if (IsParamFeatureEnabled()) {
    EXPECT_THAT(field.server_predictions(),
                ElementsAre(EqualsPrediction(crowdsourcing_prediction),
                            EqualsPrediction(ai_prediction)));
  } else {
    EXPECT_THAT(field.server_predictions(),
                ElementsAre(EqualsPrediction(crowdsourcing_prediction)));
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AutofillFieldWithAutofillAiTest);

// Parameters for `PrecedenceOverAutocompleteTest`
struct PrecedenceOverAutocompleteParams {
  // These values denote what type the field was viewed by html, server and
  // heuristic prediction.
  const HtmlFieldType html_field_type;
  const FieldType server_type;
  const FieldType heuristic_type;
  // This value denotes what `ComputedType` should return as field type.
  const FieldType expected_result;
  const AutofillPredictionSource expected_source;
};

class PrecedenceOverAutocompleteTest
    : public testing::TestWithParam<PrecedenceOverAutocompleteParams> {};

// Tests giving StreetName or HouseNumber predictions, by heuristic or server,
// precedence over HtmlFieldType::kAddressLine(1|2) autocomplete prediction.
TEST_P(PrecedenceOverAutocompleteTest, PrecedenceOverAutocompleteParams) {
  PrecedenceOverAutocompleteParams test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.html_field_type, HtmlFieldMode::kNone);
  field.set_server_predictions({CreateFieldPrediction(test_case.server_type)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           test_case.heuristic_type);
  EXPECT_THAT(field.ComputedType().GetTypes(),
              ElementsAre(test_case.expected_result));
  EXPECT_EQ(field.PredictionSource(), test_case.expected_source);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillFieldTest,
    PrecedenceOverAutocompleteTest,
    testing::Values(
        PrecedenceOverAutocompleteParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_LINE2,
            .expected_result = ADDRESS_HOME_STREET_NAME,
            .expected_source = AutofillPredictionSource::kServerCrowdsourcing},

        PrecedenceOverAutocompleteParams{
            .html_field_type = HtmlFieldType::kAddressLine1,
            .server_type = ADDRESS_HOME_LINE1,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_source = AutofillPredictionSource::kHeuristics},

        PrecedenceOverAutocompleteParams{
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = ADDRESS_HOME_STREET_NAME,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
            .expected_result = NAME_FIRST,
            .expected_source = AutofillPredictionSource::kAutocomplete}));

// Tests ensuring that ac=unrecognized fields receive predictions.
// For such fields, suggestions and filling is suppressed, which is indicated by
// a function `AutofillField::ShouldSuppressSuggestionsAndFillingByDefault()`.
// Every test specifies the predicted type for a field and what the expected
// return value of the aforementioned function is.
struct AutocompleteUnrecognizedTypeTestCase {
  // Either server or heuristic type - this doesn't matter for these tests.
  FieldType predicted_type;
  // If the predicted type should be treated as a server overwrite. Server
  // overwrites already take precedence over ac=unrecognized.
  bool is_server_overwrite = false;
  // Expected value of `ShouldSuppressSuggestionsAndFillingByDefault()`.
  bool expect_should_suppress_suggestions_and_filling;
  const AutofillPredictionSource expected_source;
};

class AutocompleteUnrecognizedTypeTest
    : public testing::TestWithParam<AutocompleteUnrecognizedTypeTestCase> {};

TEST_P(AutocompleteUnrecognizedTypeTest, TypePredictions) {
  // Create a field with ac=unrecognized and the specified predicted type.
  const AutocompleteUnrecognizedTypeTestCase& test = GetParam();
  AutofillField field;
  field.set_server_predictions(
      {CreateFieldPrediction(test.predicted_type, test.is_server_overwrite)});
  field.SetHtmlType(HtmlFieldType::kUnrecognized, HtmlFieldMode::kNone);

  // Expect that the predicted type wins over ac=unrecognized.
  EXPECT_THAT(field.Type().GetTypes(), ElementsAre(test.predicted_type));
  EXPECT_EQ(field.ShouldSuppressSuggestionsAndFillingByDefault(),
            test.expect_should_suppress_suggestions_and_filling);
  EXPECT_EQ(field.PredictionSource(), test.expected_source);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillFieldTest,
    AutocompleteUnrecognizedTypeTest,
    testing::Values(
        // Predicted address type: Expect no suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = ADDRESS_HOME_CITY,
            .expect_should_suppress_suggestions_and_filling = true,
            .expected_source = AutofillPredictionSource::kServerCrowdsourcing},
        // Server overwrite: Expect suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = ADDRESS_HOME_CITY,
            .is_server_overwrite = true,
            .expect_should_suppress_suggestions_and_filling = false,
            .expected_source = AutofillPredictionSource::kServerOverride},
        // Credit card prediction: They ignore ac=unrecognized independently of
        // the feature. Thus, expect suggestions/filling.
        AutocompleteUnrecognizedTypeTestCase{
            .predicted_type = CREDIT_CARD_NUMBER,
            .expect_should_suppress_suggestions_and_filling = false,
            .expected_source =
                AutofillPredictionSource::kServerCrowdsourcing}));

// Parameters for `AutofillLocalHeuristicsOverridesTest`
struct AutofillLocalHeuristicsOverridesParams {
  // These values denote what type the field was classified as html, server and
  // heuristic prediction.
  const HtmlFieldType html_field_type;
  const FieldType server_type;
  const FieldType heuristic_type;
  // This value denotes what `ComputedType` should return as field type.
  const FieldType expected_result;
  const AutofillPredictionSource expected_source;
};

class AutofillLocalHeuristicsOverridesTest
    : public testing::TestWithParam<AutofillLocalHeuristicsOverridesParams> {
 public:
  AutofillLocalHeuristicsOverridesTest() = default;

 private:
  base::test::ScopedFeatureList feature_{
      features::kAutofillEnableEmailOrLoyaltyCardsFilling};
};

// Tests the correctness of local heuristic overrides while computing the
// overall field type.
TEST_P(AutofillLocalHeuristicsOverridesTest,
       AutofillLocalHeuristicsOverridesParams) {
  AutofillLocalHeuristicsOverridesParams test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.html_field_type, HtmlFieldMode::kNone);
  field.set_server_predictions({CreateFieldPrediction(test_case.server_type)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           test_case.heuristic_type);
  EXPECT_THAT(field.ComputedType().GetTypes(),
              ElementsAre(test_case.expected_result))
      << "html_field_type: " << test_case.html_field_type
      << ", server_type: " << test_case.server_type
      << ", heuristic_type: " << test_case.heuristic_type
      << ", expected_result: " << test_case.expected_result;
  EXPECT_EQ(field.PredictionSource(), test_case.expected_source);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillHeuristicsOverrideTest,
    AutofillLocalHeuristicsOverridesTest,
    testing::Values(
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_CITY,
            .heuristic_type = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_result = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_APT_NUM,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_LINE1,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_result = ADDRESS_HOME_ADMIN_LEVEL2,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_APT_NUM,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel2,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_result = ADDRESS_HOME_BETWEEN_STREETS,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLevel1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_result = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine1,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_result = ADDRESS_HOME_DEPENDENT_LOCALITY,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
            .expected_result = ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAddressLine2,
            .server_type = ADDRESS_HOME_LINE2,
            .heuristic_type = ADDRESS_HOME_OVERFLOW,
            .expected_result = ADDRESS_HOME_OVERFLOW,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = EMAIL_ADDRESS,
            .heuristic_type = EMAIL_OR_LOYALTY_MEMBERSHIP_ID,
            .expected_result = EMAIL_OR_LOYALTY_MEMBERSHIP_ID,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kEmail,
            .server_type = NO_SERVER_DATA,
            .heuristic_type = EMAIL_OR_LOYALTY_MEMBERSHIP_ID,
            .expected_result = EMAIL_OR_LOYALTY_MEMBERSHIP_ID,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = NO_SERVER_DATA,
            .heuristic_type = LOYALTY_MEMBERSHIP_ID,
            .expected_result = LOYALTY_MEMBERSHIP_ID,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = UNKNOWN_TYPE,
            .heuristic_type = LOYALTY_MEMBERSHIP_ID,
            .expected_result = LOYALTY_MEMBERSHIP_ID,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnrecognized,
            .server_type = PHONE_HOME_NUMBER,
            .heuristic_type = LOYALTY_MEMBERSHIP_ID,
            .expected_result = PHONE_HOME_NUMBER,
            .expected_source = AutofillPredictionSource::kServerCrowdsourcing},
        // Test non-override behaviour.
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kStreetAddress,
            .server_type = ADDRESS_HOME_STREET_ADDRESS,
            .heuristic_type = ADDRESS_HOME_STREET_ADDRESS,
            .expected_result = ADDRESS_HOME_STREET_ADDRESS,
            .expected_source = AutofillPredictionSource::kAutocomplete},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_CITY,
            .heuristic_type = ADDRESS_HOME_APT_NUM,
            .expected_result = ADDRESS_HOME_CITY,
            .expected_source = AutofillPredictionSource::kServerCrowdsourcing},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_HOUSE_NUMBER,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = ADDRESS_HOME_APT_NUM,
            .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            .expected_result = ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_FULL,
            .heuristic_type = ALTERNATIVE_FULL_NAME,
            .expected_result = ALTERNATIVE_FULL_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_FIRST,
            .heuristic_type = ALTERNATIVE_GIVEN_NAME,
            .expected_result = ALTERNATIVE_GIVEN_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST,
            .heuristic_type = ALTERNATIVE_FAMILY_NAME,
            .expected_result = ALTERNATIVE_FAMILY_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST_SECOND,
            .heuristic_type = ALTERNATIVE_FAMILY_NAME,
            .expected_result = ALTERNATIVE_FAMILY_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST_CORE,
            .heuristic_type = ALTERNATIVE_FAMILY_NAME,
            .expected_result = ALTERNATIVE_FAMILY_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAdditionalName,
            .server_type = NAME_LAST_PREFIX,
            .heuristic_type = NAME_LAST_PREFIX,
            .expected_result = NAME_LAST_PREFIX,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kAdditionalNameInitial,
            .server_type = NAME_LAST_PREFIX,
            .heuristic_type = NAME_LAST_PREFIX,
            .expected_result = NAME_LAST_PREFIX,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kFamilyName,
            .server_type = NAME_LAST_CORE,
            .heuristic_type = NAME_LAST_CORE,
            .expected_result = NAME_LAST_CORE,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_MIDDLE,
            .heuristic_type = NAME_LAST_PREFIX,
            .expected_result = NAME_LAST_PREFIX,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_LAST,
            .heuristic_type = NAME_LAST_CORE,
            .expected_result = NAME_LAST_CORE,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_FULL,
            .heuristic_type = NAME_FIRST,
            .expected_result = NAME_FULL,
            .expected_source = AutofillPredictionSource::kServerCrowdsourcing},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NAME_FULL,
            .heuristic_type = UNKNOWN_TYPE,
            .expected_result = NAME_FULL,
            .expected_source = AutofillPredictionSource::kServerCrowdsourcing},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kName,
            .server_type = NO_SERVER_DATA,
            .heuristic_type = ALTERNATIVE_FULL_NAME,
            .expected_result = ALTERNATIVE_FULL_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kGivenName,
            .server_type = NO_SERVER_DATA,
            .heuristic_type = ALTERNATIVE_GIVEN_NAME,
            .expected_result = ALTERNATIVE_GIVEN_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics},
        AutofillLocalHeuristicsOverridesParams{
            .html_field_type = HtmlFieldType::kFamilyName,
            .server_type = NO_SERVER_DATA,
            .heuristic_type = ALTERNATIVE_FAMILY_NAME,
            .expected_result = ALTERNATIVE_FAMILY_NAME,
            .expected_source = AutofillPredictionSource::kHeuristics}));

// Tests that consecutive identical events are not added twice to the event log.
TEST(AutofillFieldLogEventTypeTest, AppendLogEventIfNotRepeated) {
  // The following three FieldLogEventTypes are arbitrary besides being of
  // distinct types.
  AutofillField::FieldLogEventType a = AskForValuesToFillFieldLogEvent{
      .has_suggestion = OptionalBoolean::kFalse,
      .suggestion_is_shown = OptionalBoolean::kFalse};
  AutofillField::FieldLogEventType a2 = AskForValuesToFillFieldLogEvent{
      .has_suggestion = OptionalBoolean::kTrue,
      .suggestion_is_shown = OptionalBoolean::kTrue};
  AutofillField::FieldLogEventType b =
      TriggerFillFieldLogEvent{.data_type = FillDataType::kUndefined,
                               .associated_country_code = "DE",
                               .timestamp = AutofillClock::Now()};
  AutofillField::FieldLogEventType c = FillFieldLogEvent{
      .fill_event_id = std::get<TriggerFillFieldLogEvent>(b).fill_event_id,
      .had_value_before_filling = OptionalBoolean::kTrue,
      .autofill_skipped_status = FieldFillingSkipReason::kAlreadyAutofilled,
      .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue};

  AutofillField f;
  EXPECT_TRUE(f.field_log_events().empty());

  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 1u);
  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 1u);

  f.AppendLogEventIfNotRepeated(a2);
  EXPECT_EQ(f.field_log_events().size(), 2u);
  f.AppendLogEventIfNotRepeated(a2);
  EXPECT_EQ(f.field_log_events().size(), 2u);

  f.AppendLogEventIfNotRepeated(b);
  EXPECT_EQ(f.field_log_events().size(), 3u);
  f.AppendLogEventIfNotRepeated(b);
  EXPECT_EQ(f.field_log_events().size(), 3u);

  f.AppendLogEventIfNotRepeated(c);
  EXPECT_EQ(f.field_log_events().size(), 4u);
  f.AppendLogEventIfNotRepeated(c);
  EXPECT_EQ(f.field_log_events().size(), 4u);

  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 5u);
  f.AppendLogEventIfNotRepeated(a);
  EXPECT_EQ(f.field_log_events().size(), 5u);
}

TEST(AutofillPredictionSourceToStringViewTest, ConversionTest) {
  EXPECT_EQ(AutofillPredictionSourceToStringView(
                AutofillPredictionSource::kHeuristics),
            "Heuristics");
  EXPECT_EQ(AutofillPredictionSourceToStringView(
                AutofillPredictionSource::kAutocomplete),
            "AutocompleteAttribute");
  EXPECT_EQ(AutofillPredictionSourceToStringView(
                AutofillPredictionSource::kServerCrowdsourcing),
            "ServerCrowdsourcing");
  EXPECT_EQ(AutofillPredictionSourceToStringView(
                AutofillPredictionSource::kServerOverride),
            "ServerOverride");
  EXPECT_EQ(AutofillPredictionSourceToStringView(
                AutofillPredictionSource::kRationalization),
            "Rationalization");
}

// Parameters for `AutofillFieldTestWithPasswordManagerMlPredictions`
struct AutofillFieldWithPasswordManagerMlPredictionsParams {
  // These values denote what type the field was classified as by various
  // sources.
  const HtmlFieldType html_field_type;
  const FieldType server_type;
  const FieldType autofill_heuristic_type;
  const FieldType password_manager_predicted_type;
  // These value denote expected resulting type and prediction source.
  const FieldType expected_result;
  const AutofillPredictionSource expected_source;
};

class AutofillFieldTestWithPasswordManagerMlPredictions
    : public testing::TestWithParam<
          AutofillFieldWithPasswordManagerMlPredictionsParams> {};

// Tests the correctness of applying password manager ML predictions while
// computing the overall field type.
TEST_P(AutofillFieldTestWithPasswordManagerMlPredictions,
       PasswordManagerMlPredictions) {
  AutofillFieldWithPasswordManagerMlPredictionsParams test_case = GetParam();
  SCOPED_TRACE(testing::Message()
               << "html_field_type: " << test_case.html_field_type
               << ", server_type: " << test_case.server_type
               << ", autofill_heuristic_type: "
               << test_case.autofill_heuristic_type
               << ", password_manager_predicted_type: "
               << test_case.password_manager_predicted_type
               << ", expected_result: " << test_case.expected_result);

  AutofillField field;
  field.SetHtmlType(test_case.html_field_type, HtmlFieldMode::kNone);
  field.set_server_predictions({CreateFieldPrediction(test_case.server_type)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           test_case.autofill_heuristic_type);
  field.set_heuristic_type(HeuristicSource::kPasswordManagerMachineLearning,
                           test_case.password_manager_predicted_type);
  EXPECT_THAT(field.ComputedType().GetTypes(),
              ElementsAre(test_case.expected_result));
  EXPECT_EQ(field.PredictionSource(), test_case.expected_source);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillFieldTestWithPasswordManagerMlPredictions,
    AutofillFieldTestWithPasswordManagerMlPredictions,
    testing::Values(
        // Verify that autofill server predictions override password manager
        // OTP classifications.
        AutofillFieldWithPasswordManagerMlPredictionsParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = PHONE_HOME_NUMBER,
            .autofill_heuristic_type = UNKNOWN_TYPE,
            .password_manager_predicted_type = ONE_TIME_CODE,
            .expected_result = PHONE_HOME_NUMBER,
            .expected_source = AutofillPredictionSource::kServerCrowdsourcing},
        // Verify that password manager server predictions do not override
        // password manager OTP classifications.
        AutofillFieldWithPasswordManagerMlPredictionsParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = PASSWORD,
            .autofill_heuristic_type = UNKNOWN_TYPE,
            .password_manager_predicted_type = ONE_TIME_CODE,
            .expected_result = ONE_TIME_CODE,
            .expected_source = AutofillPredictionSource::kHeuristics},
        // Verify that autocomplete has higher priority than password manager
        // OTP classifications.
        AutofillFieldWithPasswordManagerMlPredictionsParams{
            .html_field_type = HtmlFieldType::kPostalCode,
            .server_type = NO_SERVER_DATA,
            .autofill_heuristic_type = UNKNOWN_TYPE,
            .password_manager_predicted_type = ONE_TIME_CODE,
            .expected_result = ADDRESS_HOME_ZIP,
            .expected_source = AutofillPredictionSource::kAutocomplete},
        // Verify that autocomplete has higher priority than password manager
        // non-OTP classifications.
        AutofillFieldWithPasswordManagerMlPredictionsParams{
            .html_field_type = HtmlFieldType::kPostalCode,
            .server_type = NO_SERVER_DATA,
            .autofill_heuristic_type = UNKNOWN_TYPE,
            .password_manager_predicted_type = PASSWORD,
            .expected_result = ADDRESS_HOME_ZIP,
            .expected_source = AutofillPredictionSource::kAutocomplete},
        // Verify that password manager OTP classifications override autofill
        // heuristics.
        AutofillFieldWithPasswordManagerMlPredictionsParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NO_SERVER_DATA,
            .autofill_heuristic_type = PHONE_HOME_NUMBER,
            .password_manager_predicted_type = ONE_TIME_CODE,
            .expected_result = ONE_TIME_CODE,
            .expected_source = AutofillPredictionSource::kHeuristics},
        // Verify that password manager non-OTP classifications do not override
        // autofill heuristics.
        AutofillFieldWithPasswordManagerMlPredictionsParams{
            .html_field_type = HtmlFieldType::kUnspecified,
            .server_type = NO_SERVER_DATA,
            .autofill_heuristic_type = PHONE_HOME_NUMBER,
            .password_manager_predicted_type = PASSWORD,
            .expected_result = PHONE_HOME_NUMBER,
            .expected_source = AutofillPredictionSource::kHeuristics}));

}  // namespace
}  // namespace autofill
