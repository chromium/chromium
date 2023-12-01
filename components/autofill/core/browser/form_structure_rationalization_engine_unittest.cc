// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalization_engine.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::rationalization {
namespace {

using ::testing::ElementsAre;

BASE_FEATURE(kTestFeatureForFormStructureRationalizationEngine,
             "TestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This should be merged with the logic in
// form_structure_rationalizer_unittest.cc but our code style does not allow
// designated list initialization for complex structs, so we cannot move the
// struct into a shared header. Therefore, this is a minimally viable copy
// of what's offered in form_structure_rationalizer_unittest.cc.
struct FieldTemplate {
  std::u16string label;
  std::u16string name;
  ServerFieldType field_type = UNKNOWN_TYPE;
};

std::vector<std::unique_ptr<AutofillField>> CreateFields(
    const std::vector<FieldTemplate>& field_templates) {
  std::vector<std::unique_ptr<AutofillField>> result;
  result.reserve(field_templates.size());
  for (const auto& t : field_templates) {
    const auto& f =
        result.emplace_back(std::make_unique<AutofillField>(FormFieldData()));
    f->name = t.name;
    f->label = t.label;
    f->SetTypeTo(AutofillType(t.field_type));
    DCHECK_EQ(f->Type().GetStorableType(), t.field_type);
  }
  return result;
}

std::vector<ServerFieldType> GetTypes(
    const std::vector<std::unique_ptr<AutofillField>>& fields) {
  std::vector<ServerFieldType> server_types;
  base::ranges::transform(
      fields, std::back_inserter(server_types),
      [](const auto& field) { return field->Type().GetStorableType(); });
  return server_types;
}

RationalizationRule CreateTestRule() {
  return RationalizationRuleBuilder()
      .SetRuleName("Fix colonia as address-line2 in MX")
      .SetEnvironmentCondition(
          EnvironmentConditionBuilder()
              .SetCountryList({GeoIpCountryCode("MX")})
              .SetFeature(&kTestFeatureForFormStructureRationalizationEngine)
              .Build())
      .SetTriggerField(FieldCondition{
          .possible_overall_types = ServerFieldTypeSet{ADDRESS_HOME_LINE2},
          .regex_reference_match = "ADDRESS_HOME_DEPENDENT_LOCALITY",
      })
      .SetOtherFieldConditions({
          FieldCondition{
              .location = FieldLocation::kLastClassifiedPredecessor,
              .possible_overall_types = ServerFieldTypeSet{ADDRESS_HOME_LINE1},
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
}

TEST(FormStructureRationalizationEngine, TestBuilder) {
  RationalizationRule rule = CreateTestRule();

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

// Test that the actions are applied if all conditions are met.
TEST(FormStructureRationalizationEngine, TestRulesAreApplied) {
  base::test::ScopedFeatureList feature_list(
      kTestFeatureForFormStructureRationalizationEngine);

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Nombre", u"n", NAME_FIRST},
      {u"Apellidos", u"a", NAME_LAST},
      {u"Empresa", u"empresa", COMPANY_NAME},
      {u"Dirección", u"addressline1", ADDRESS_HOME_LINE1},
      {u"Colonia", u"addressline2", ADDRESS_HOME_LINE2},
      {u"Código postal", u"postalcode", ADDRESS_HOME_ZIP},
      {u"Cuidad", u"city", ADDRESS_HOME_CITY},
      {u"Estado", u"state", ADDRESS_HOME_STATE},
  });

  internal::ApplyRuleIfApplicable(CreateTestRule(), GeoIpCountryCode("MX"),
                                  LanguageCode("es"), PatternSource::kLegacy,
                                  fields);

  EXPECT_THAT(
      GetTypes(fields),
      ElementsAre(NAME_FIRST, NAME_LAST, COMPANY_NAME,
                  /*changed*/ ADDRESS_HOME_STREET_ADDRESS,
                  /*changed*/ ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_ZIP,
                  ADDRESS_HOME_CITY, ADDRESS_HOME_STATE));
}

// Test that no actions are applied if the trigger field does not exist.
TEST(FormStructureRationalizationEngine,
     TestRulesAreNotAppliedWithMissingTriggerField) {
  base::test::ScopedFeatureList feature_list(
      kTestFeatureForFormStructureRationalizationEngine);

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Nombre", u"n", NAME_FIRST},
      {u"Apellidos", u"a", NAME_LAST},
      {u"Empresa", u"empresa", COMPANY_NAME},
      {u"Dirección", u"addressline1", ADDRESS_HOME_LINE1},
      /*{u"Colonia", u"addressline2", ADDRESS_HOME_LINE2},*/
      {u"Código postal", u"postalcode", ADDRESS_HOME_ZIP},
      {u"Cuidad", u"city", ADDRESS_HOME_CITY},
      {u"Estado", u"state", ADDRESS_HOME_STATE},
  });

  internal::ApplyRuleIfApplicable(CreateTestRule(), GeoIpCountryCode("MX"),
                                  LanguageCode("es"), PatternSource::kLegacy,
                                  fields);

  EXPECT_THAT(
      GetTypes(fields),
      ElementsAre(NAME_FIRST, NAME_LAST, COMPANY_NAME, ADDRESS_HOME_LINE1,
                  /*ADDRESS_HOME_LINE2,*/ ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY,
                  ADDRESS_HOME_STATE));
}

// Test that no actions are applied if the additional condition field does not
// exist.
TEST(FormStructureRationalizationEngine,
     TestRulesAreNotAppliedWithMissingAdditionalCondition) {
  base::test::ScopedFeatureList feature_list(
      kTestFeatureForFormStructureRationalizationEngine);

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Nombre", u"n", NAME_FIRST},
      {u"Apellidos", u"a", NAME_LAST},
      {u"Empresa", u"empresa", COMPANY_NAME},
      /*{u"Dirección", u"addressline1", ADDRESS_HOME_LINE1},*/
      {u"Colonia", u"addressline2", ADDRESS_HOME_LINE2},
      {u"Código postal", u"postalcode", ADDRESS_HOME_ZIP},
      {u"Cuidad", u"city", ADDRESS_HOME_CITY},
      {u"Estado", u"state", ADDRESS_HOME_STATE},
  });

  internal::ApplyRuleIfApplicable(CreateTestRule(), GeoIpCountryCode("MX"),
                                  LanguageCode("es"), PatternSource::kLegacy,
                                  fields);

  EXPECT_THAT(
      GetTypes(fields),
      ElementsAre(NAME_FIRST, NAME_LAST, COMPANY_NAME, /*ADDRESS_HOME_LINE1,*/
                  ADDRESS_HOME_LINE2, ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY,
                  ADDRESS_HOME_STATE));
}

// Test that no actions are applied if the additional condition asks for
// a direct classified predecessor but the field meeting the condition is not
// a direct predecessor.
TEST(FormStructureRationalizationEngine,
     TestRulesAreNotAppliedWithViolatedDirectPredecessorRule) {
  base::test::ScopedFeatureList feature_list(
      kTestFeatureForFormStructureRationalizationEngine);

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Nombre", u"n", NAME_FIRST},
      {u"Apellidos", u"a", NAME_LAST},
      // Address line 1 is not a direct classified predecessor, so it is not
      // found.
      {u"Dirección", u"addressline1", ADDRESS_HOME_LINE1},
      {u"Empresa", u"empresa", COMPANY_NAME},
      {u"Colonia", u"addressline2", ADDRESS_HOME_LINE2},
      {u"Código postal", u"postalcode", ADDRESS_HOME_ZIP},
      {u"Cuidad", u"city", ADDRESS_HOME_CITY},
      {u"Estado", u"state", ADDRESS_HOME_STATE},
  });

  internal::ApplyRuleIfApplicable(CreateTestRule(), GeoIpCountryCode("MX"),
                                  LanguageCode("es"), PatternSource::kLegacy,
                                  fields);

  EXPECT_THAT(GetTypes(fields),
              ElementsAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_LINE1,
                          COMPANY_NAME, ADDRESS_HOME_LINE2, ADDRESS_HOME_ZIP,
                          ADDRESS_HOME_CITY, ADDRESS_HOME_STATE));
}

// Test that the kLastClassifiedPredecessor can skip unclassified predecessors.
TEST(FormStructureRationalizationEngine,
     TestRulesAreAppliedIfLastClassifiedPredecessorNeedsToSkipAField) {
  base::test::ScopedFeatureList feature_list(
      kTestFeatureForFormStructureRationalizationEngine);

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Nombre", u"n", NAME_FIRST},
      {u"Apellidos", u"a", NAME_LAST},
      {u"Empresa", u"empresa", COMPANY_NAME},
      {u"Dirección", u"addressline1", ADDRESS_HOME_LINE1},
      // The UNKNOWN_TYPE can be skipped for a
      // FieldLocation::kLastClassifiedPredecessor.
      {u"Foo", u"bar", UNKNOWN_TYPE},
      {u"Colonia", u"addressline2", ADDRESS_HOME_LINE2},
      {u"Código postal", u"postalcode", ADDRESS_HOME_ZIP},
      {u"Cuidad", u"city", ADDRESS_HOME_CITY},
      {u"Estado", u"state", ADDRESS_HOME_STATE},
  });

  internal::ApplyRuleIfApplicable(CreateTestRule(), GeoIpCountryCode("MX"),
                                  LanguageCode("es"), PatternSource::kLegacy,
                                  fields);

  EXPECT_THAT(
      GetTypes(fields),
      ElementsAre(NAME_FIRST, NAME_LAST, COMPANY_NAME,
                  /*changed*/ ADDRESS_HOME_STREET_ADDRESS, UNKNOWN_TYPE,
                  /*changed*/ ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_ZIP,
                  ADDRESS_HOME_CITY, ADDRESS_HOME_STATE));
}

}  // namespace
}  // namespace autofill::rationalization
