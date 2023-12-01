// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalization_engine.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::rationalization {
namespace {

BASE_FEATURE(kTestFeatureForFormStructureRationalizationEngine,
             "TestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Verify that the builder classes set all parameters correctly.
TEST(FormStructureRationalizationEngine, TestBuilder) {
  RationalizationRule rule =
      RationalizationRuleBuilder()
          .SetRuleName("Fix colonia as address-line2 in MX")
          .SetEnvironmentCondition(
              EnvironmentConditionBuilder()
                  .SetCountryList({GeoIpCountryCode("MX")})
                  .SetFeature(
                      &kTestFeatureForFormStructureRationalizationEngine)
                  .Build())
          .SetTriggerField(FieldCondition{
              .possible_overall_types = ServerFieldTypeSet{ADDRESS_HOME_LINE2},
              .regex_reference_match = "ADDRESS_HOME_DEPENDENT_LOCALITY",
          })
          .SetOtherFieldConditions({
              FieldCondition{
                  .location = FieldLocation::kLastClassifiedPredecessor,
                  .possible_overall_types =
                      ServerFieldTypeSet{ADDRESS_HOME_LINE1},
              },
          })
          .SetActions({
              SetTypeAction{
                  .target = FieldLocation::kLastClassifiedPredecessor,
                  .set_overall_type = ADDRESS_HOME_STREET_ADDRESS,
              },
              SetTypeAction{
                  .target = FieldLocation::kTriggerField,
                  .set_overall_type = ADDRESS_HOME_DEPENDENT_LOCALITY,
              },
          })
          .Build();

  EXPECT_EQ(rule.rule_name, "Fix colonia as address-line2 in MX");

  ASSERT_TRUE(rule.environment_condition.has_value());
  EXPECT_EQ(rule.environment_condition->feature,
            &kTestFeatureForFormStructureRationalizationEngine);
  EXPECT_THAT(rule.environment_condition->country_list,
              testing::ElementsAre(GeoIpCountryCode("MX")));

  EXPECT_EQ(rule.trigger_field.location, FieldLocation::kTriggerField);
  EXPECT_EQ(rule.trigger_field.possible_overall_types,
            ServerFieldTypeSet{ADDRESS_HOME_LINE2});
  EXPECT_EQ(rule.trigger_field.regex_reference_match,
            "ADDRESS_HOME_DEPENDENT_LOCALITY");

  ASSERT_EQ(rule.other_field_conditions.size(), 1u);
  EXPECT_EQ(rule.other_field_conditions[0].location,
            FieldLocation::kLastClassifiedPredecessor);
  EXPECT_EQ(rule.other_field_conditions[0].possible_overall_types,
            ServerFieldTypeSet{ADDRESS_HOME_LINE1});

  ASSERT_EQ(rule.actions.size(), 2u);
  EXPECT_EQ(rule.actions[0].target, FieldLocation::kLastClassifiedPredecessor);
  EXPECT_EQ(rule.actions[0].set_overall_type, ADDRESS_HOME_STREET_ADDRESS);
  EXPECT_EQ(rule.actions[1].target, FieldLocation::kTriggerField);
  EXPECT_EQ(rule.actions[1].set_overall_type, ADDRESS_HOME_DEPENDENT_LOCALITY);
}

// Verifies that the client country is correctly handled by
// IsEnvironmentConditionFulfilled.
TEST(FormStructureRationalizationEngine,
     IsEnvironmentConditionFulfilled_CheckCountry) {
  using internal::IsEnvironmentConditionFulfilled;
  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  GeoIpCountryCode kBR = GeoIpCountryCode("BR");
  GeoIpCountryCode kUS = GeoIpCountryCode("US");

  EnvironmentCondition no_country_required =
      EnvironmentConditionBuilder().Build();
  EXPECT_TRUE(IsEnvironmentConditionFulfilled(no_country_required, kMX));

  EnvironmentCondition specific_country_required =
      EnvironmentConditionBuilder().SetCountryList({kMX}).Build();
  EXPECT_TRUE(IsEnvironmentConditionFulfilled(specific_country_required, kMX));
  EXPECT_FALSE(IsEnvironmentConditionFulfilled(specific_country_required, kBR));

  EnvironmentCondition one_of_many =
      EnvironmentConditionBuilder().SetCountryList({kBR, kMX}).Build();
  EXPECT_TRUE(IsEnvironmentConditionFulfilled(one_of_many, kBR));
  EXPECT_TRUE(IsEnvironmentConditionFulfilled(one_of_many, kMX));
  EXPECT_FALSE(IsEnvironmentConditionFulfilled(one_of_many, kUS));
}

// Verifies that the experiment state is checked.
TEST(FormStructureRationalizationEngine,
     IsEnvironmentConditionFulfilled_CheckExperiment) {
  using internal::IsEnvironmentConditionFulfilled;
  GeoIpCountryCode kMX = GeoIpCountryCode("MX");

  EnvironmentCondition no_experiment_required =
      EnvironmentConditionBuilder().Build();
  EnvironmentCondition experiment_required =
      EnvironmentConditionBuilder()
          .SetFeature(&kTestFeatureForFormStructureRationalizationEngine)
          .Build();

  {
    base::test::ScopedFeatureList enable_feature(
        kTestFeatureForFormStructureRationalizationEngine);
    EXPECT_TRUE(IsEnvironmentConditionFulfilled(no_experiment_required, kMX));
    EXPECT_TRUE(IsEnvironmentConditionFulfilled(experiment_required, kMX));
  }
  {
    base::test::ScopedFeatureList disable_feature;
    disable_feature.InitAndDisableFeature(
        kTestFeatureForFormStructureRationalizationEngine);
    EXPECT_TRUE(IsEnvironmentConditionFulfilled(no_experiment_required, kMX));
    EXPECT_FALSE(IsEnvironmentConditionFulfilled(experiment_required, kMX));
  }
}

// Verifies that the possible types are correctly checked in
// IsFieldConditionFulfilledIgnoringLocation.
TEST(FormStructureRationalizationEngine,
     IsFieldConditionFulfilledIgnoringLocation_CheckPossibleTypes) {
  using internal::IsFieldConditionFulfilledIgnoringLocation;
  GeoIpCountryCode kMX = GeoIpCountryCode("MX");

  FieldCondition no_possible_types_required = {};
  FieldCondition requires_address_line1_type = {
      .possible_overall_types = ServerFieldTypeSet{ADDRESS_HOME_LINE1},
  };

  LanguageCode page_language = LanguageCode("es");
  PatternSource pattern_source = PatternSource::kLegacy;

  AutofillField field;

  // Unknown type.
  ASSERT_EQ(field.Type().GetStorableType(), UNKNOWN_TYPE);
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      no_possible_types_required, page_language, pattern_source, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      requires_address_line1_type, page_language, pattern_source, field));

  // Non-matching type.
  field.set_heuristic_type(HeuristicSource::kLegacy, NAME_FIRST);
  ASSERT_EQ(field.Type().GetStorableType(), NAME_FIRST);
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      no_possible_types_required, page_language, pattern_source, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      requires_address_line1_type, page_language, pattern_source, field));

  // Matching type.
  field.set_heuristic_type(HeuristicSource::kLegacy, ADDRESS_HOME_LINE1);
  ASSERT_EQ(field.Type().GetStorableType(), ADDRESS_HOME_LINE1);
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      no_possible_types_required, page_language, pattern_source, field));
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      requires_address_line1_type, page_language, pattern_source, field));
}

// Verifies that the required match for regexes works as expected in
// IsFieldConditionFulfilledIgnoringLocation.
TEST(FormStructureRationalizationEngine,
     IsFieldConditionFulfilledIgnoringLocation_CheckRegex) {
  using internal::IsFieldConditionFulfilledIgnoringLocation;
  GeoIpCountryCode kMX = GeoIpCountryCode("MX");

  FieldCondition no_regex_match_required = {};
  FieldCondition requires_dependent_locality_match = {
      .regex_reference_match = "ADDRESS_HOME_DEPENDENT_LOCALITY",
  };

  LanguageCode page_language = LanguageCode("es");
  PatternSource pattern_source = PatternSource::kLegacy;

  AutofillField field;
  field.label = u"";

  // Empty label.
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      no_regex_match_required, page_language, pattern_source, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      requires_dependent_locality_match, page_language, pattern_source, field));

  // Non-matching label.
  field.label = u"foobar";
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      no_regex_match_required, page_language, pattern_source, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      requires_dependent_locality_match, page_language, pattern_source, field));

  // Matching label.
  field.label = u"colonia";
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      no_regex_match_required, page_language, pattern_source, field));
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      requires_dependent_locality_match, page_language, pattern_source, field));

  // Matching label but incorrect type.
  field.label = u"colonia";
  field.form_control_type = FormControlType::kInputMonth;
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      no_regex_match_required, page_language, pattern_source, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      requires_dependent_locality_match, page_language, pattern_source, field));

  FieldCondition regex_with_negative_pattern = {
      .regex_reference_match = "ADDRESS_NAME_IGNORED",
  };
  // This matches the positive pattern due to "nombre.*dirección" but also
  // the negataive pattern due to "correo". Therefore, the condition should not
  // be considered fulfilled.
  field.label = u"nombre de usuario/dirección de correo electrónico";
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      regex_with_negative_pattern, page_language, pattern_source, field));
}

}  // namespace
}  // namespace autofill::rationalization
