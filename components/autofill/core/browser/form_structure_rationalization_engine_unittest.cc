// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalization_engine.h"

#include "base/containers/to_vector.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::rationalization {
namespace {

using ::testing::Contains;
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
  FieldType field_type = UNKNOWN_TYPE;
};

std::vector<std::unique_ptr<AutofillField>> CreateFields(
    const std::vector<FieldTemplate>& field_templates) {
  std::vector<std::unique_ptr<AutofillField>> result;
  result.reserve(field_templates.size());
  for (const auto& t : field_templates) {
    const auto& f =
        result.emplace_back(std::make_unique<AutofillField>(FormFieldData()));
    f->set_name(t.name);
    f->set_label(t.label);
    f->SetTypeTo(AutofillType(t.field_type),
                 AutofillPredictionSource::kHeuristics);
    DCHECK(f->Type().GetTypes().contains(t.field_type));
    DCHECK_EQ(f->Type().GetTypes(), FieldTypeSet({t.field_type}));
  }
  return result;
}

std::vector<FieldTypeSet> GetTypes(
    const std::vector<std::unique_ptr<AutofillField>>& fields) {
  std::vector<FieldTypeSet> types;
  for (const auto& field : fields) {
    types.push_back(field->Type().GetTypes());
  }
  return types;
}

auto FieldTypesAre(auto... types) {
  return ElementsAre(FieldTypeSet{types}...);
}

PatternFile GetPatternFile() {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  return PatternFile::kDefault;
#else
  return PatternFile::kLegacy;
#endif
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
          .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE2},
          .regex_reference_match = "ADDRESS_HOME_DEPENDENT_LOCALITY",
      })
      .SetOtherFieldConditions({
          FieldCondition{
              .location = FieldLocation::kLastClassifiedPredecessor,
              .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
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
              ElementsAre(GeoIpCountryCode("MX")));

  EXPECT_EQ(rule.trigger_field.location, FieldLocation::kTriggerField);
  EXPECT_EQ(rule.trigger_field.possible_overall_types,
            FieldTypeSet{ADDRESS_HOME_LINE2});
  EXPECT_EQ(rule.trigger_field.regex_reference_match,
            "ADDRESS_HOME_DEPENDENT_LOCALITY");

  ASSERT_EQ(rule.other_field_conditions.size(), 1u);
  EXPECT_EQ(rule.other_field_conditions[0].location,
            FieldLocation::kLastClassifiedPredecessor);
  EXPECT_EQ(rule.other_field_conditions[0].possible_overall_types,
            FieldTypeSet{ADDRESS_HOME_LINE1});

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
  ParsingContext kMXContext(std::vector<FormFieldData>{}, kMX,
                            LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ParsingContext kBRContext(std::vector<FormFieldData>{}, kBR,
                            LanguageCode("pt"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ParsingContext kUSContext(std::vector<FormFieldData>{}, kUS,
                            LanguageCode("en"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);

  EnvironmentCondition no_country_required =
      EnvironmentConditionBuilder().Build();
  EXPECT_TRUE(IsEnvironmentConditionFulfilled(kMXContext, no_country_required));

  EnvironmentCondition specific_country_required =
      EnvironmentConditionBuilder().SetCountryList({kMX}).Build();
  EXPECT_TRUE(
      IsEnvironmentConditionFulfilled(kMXContext, specific_country_required));
  EXPECT_FALSE(
      IsEnvironmentConditionFulfilled(kBRContext, specific_country_required));

  EnvironmentCondition one_of_many =
      EnvironmentConditionBuilder().SetCountryList({kBR, kMX}).Build();
  EXPECT_TRUE(IsEnvironmentConditionFulfilled(kBRContext, one_of_many));
  EXPECT_TRUE(IsEnvironmentConditionFulfilled(kMXContext, one_of_many));
  EXPECT_FALSE(IsEnvironmentConditionFulfilled(kUSContext, one_of_many));
}

// Verifies that the experiment state is checked.
TEST(FormStructureRationalizationEngine,
     IsEnvironmentConditionFulfilled_CheckExperiment) {
  using internal::IsEnvironmentConditionFulfilled;
  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(std::vector<FormFieldData>{}, kMX,
                            LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);

  EnvironmentCondition no_experiment_required =
      EnvironmentConditionBuilder().Build();
  EnvironmentCondition experiment_required =
      EnvironmentConditionBuilder()
          .SetFeature(&kTestFeatureForFormStructureRationalizationEngine)
          .Build();

  {
    base::test::ScopedFeatureList enable_feature(
        kTestFeatureForFormStructureRationalizationEngine);
    EXPECT_TRUE(
        IsEnvironmentConditionFulfilled(kMXContext, no_experiment_required));
    EXPECT_TRUE(
        IsEnvironmentConditionFulfilled(kMXContext, experiment_required));
  }
  {
    base::test::ScopedFeatureList disable_feature;
    disable_feature.InitAndDisableFeature(
        kTestFeatureForFormStructureRationalizationEngine);
    EXPECT_TRUE(
        IsEnvironmentConditionFulfilled(kMXContext, no_experiment_required));
    EXPECT_FALSE(
        IsEnvironmentConditionFulfilled(kMXContext, experiment_required));
  }
}

// Verifies that the possible types are correctly checked in
// IsFieldConditionFulfilledIgnoringLocation.
TEST(FormStructureRationalizationEngine,
     IsFieldConditionFulfilledIgnoringLocation_CheckPossibleTypes) {
  using internal::IsFieldConditionFulfilledIgnoringLocation;
  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(std::vector<FormFieldData>{}, kMX,
                            LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);

  FieldCondition no_possible_types_required = {};
  FieldCondition requires_address_line1_type = {
      .possible_overall_types = FieldTypeSet{ADDRESS_HOME_LINE1},
  };

  AutofillField field;

  // Unknown type.
  ASSERT_EQ(field.Type().GetAddressType(), UNKNOWN_TYPE);
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, no_possible_types_required, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, requires_address_line1_type, field));

  // Non-matching type.
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);
  ASSERT_EQ(field.Type().GetAddressType(), NAME_FIRST);
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, no_possible_types_required, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, requires_address_line1_type, field));

  // Matching type.
  field.set_heuristic_type(GetActiveHeuristicSource(), ADDRESS_HOME_LINE1);
  ASSERT_EQ(field.Type().GetAddressType(), ADDRESS_HOME_LINE1);
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, no_possible_types_required, field));
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, requires_address_line1_type, field));
}

// Verifies that the required match for regexes works as expected in
// IsFieldConditionFulfilledIgnoringLocation.
TEST(FormStructureRationalizationEngine,
     IsFieldConditionFulfilledIgnoringLocation_CheckRegex) {
  using internal::IsFieldConditionFulfilledIgnoringLocation;
  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(std::vector<FormFieldData>{}, kMX,
                            LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);

  FieldCondition no_regex_match_required = {};
  FieldCondition requires_dependent_locality_match = {
      .regex_reference_match = "ADDRESS_HOME_DEPENDENT_LOCALITY",
  };

  AutofillField field;
  field.set_label(u"");

  // Empty label.
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, no_regex_match_required, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, requires_dependent_locality_match, field));

  // Non-matching label.
  field.set_label(u"foobar");
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, no_regex_match_required, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, requires_dependent_locality_match, field));

  // Matching label.
  field.set_label(u"colonia");
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, no_regex_match_required, field));
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, requires_dependent_locality_match, field));

  // Matching label but incorrect type.
  field.set_label(u"colonia");
  field.set_form_control_type(FormControlType::kInputMonth);
  EXPECT_TRUE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, no_regex_match_required, field));
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, requires_dependent_locality_match, field));

  FieldCondition regex_with_negative_pattern = {
      .regex_reference_match = "ADDRESS_NAME_IGNORED",
  };
  // This matches the positive pattern due to "nombre.*dirección" but also
  // the negataive pattern due to "correo". Therefore, the condition should not
  // be considered fulfilled.
  field.set_label(u"nombre de usuario/dirección de correo electrónico");
  EXPECT_FALSE(IsFieldConditionFulfilledIgnoringLocation(
      kMXContext, regex_with_negative_pattern, field));
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

  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(fields, kMX, LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  internal::ApplyRuleIfApplicable(kMXContext, CreateTestRule(), fields);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST, COMPANY_NAME,
                    /*changed*/ ADDRESS_HOME_STREET_ADDRESS,
                    /*changed*/ ADDRESS_HOME_DEPENDENT_LOCALITY,
                    ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE));
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

  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(fields, kMX, LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  internal::ApplyRuleIfApplicable(kMXContext, CreateTestRule(), fields);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST, COMPANY_NAME, ADDRESS_HOME_LINE1,
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

  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(fields, kMX, LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  internal::ApplyRuleIfApplicable(kMXContext, CreateTestRule(), fields);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST, COMPANY_NAME, /*ADDRESS_HOME_LINE1,*/
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

  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(fields, kMX, LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  internal::ApplyRuleIfApplicable(kMXContext, CreateTestRule(), fields);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_LINE1,
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

  GeoIpCountryCode kMX = GeoIpCountryCode("MX");
  ParsingContext kMXContext(fields, kMX, LanguageCode("es"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  internal::ApplyRuleIfApplicable(kMXContext, CreateTestRule(), fields);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST, COMPANY_NAME,
                    /*changed*/ ADDRESS_HOME_STREET_ADDRESS, UNKNOWN_TYPE,
                    /*changed*/ ADDRESS_HOME_DEPENDENT_LOCALITY,
                    ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE));
}

// Test that the actions are applied if all conditions are met.
TEST(FormStructureRationalizationEngine, TestDEOverflowRuleIsApplied) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Name", u"n", NAME_FIRST},
      {u"Nachname", u"a", NAME_LAST},
      {u"Straße und Hausnummer", u"addressline1", ADDRESS_HOME_STREET_LOCATION},
      {u"Adresszusatz", u"adresszusatz", ADDRESS_HOME_OVERFLOW},
      {u"PLZ", u"plz", ADDRESS_HOME_ZIP},
      {u"Ort", u"ort", ADDRESS_HOME_CITY},
  });

  GeoIpCountryCode kDE = GeoIpCountryCode("DE");
  ParsingContext kDEContext(fields, kDE, LanguageCode("de"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kDEContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST,
                            /*changed*/ ADDRESS_HOME_LINE1,
                            /*changed*/ ADDRESS_HOME_LINE2, ADDRESS_HOME_ZIP,
                            ADDRESS_HOME_CITY));
}

// Test that a house number field not followed by an apartment is treated
// as a ADDRESS_HOME_HOUSE_NUMBER_AND_APT in Poland.
TEST(FormStructureRationalizationEngine, TestPLHouseNumberAndAptChanged) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Imię", u"n", NAME_FIRST},
      {u"Nachname", u"a", NAME_LAST},
      {u"Ulica", u"Ulica", ADDRESS_HOME_STREET_NAME},
      {u"Nomeru domu", u"nomeru domu", ADDRESS_HOME_HOUSE_NUMBER},
      {u"KOD", u"kod", ADDRESS_HOME_ZIP},
      {u"Miejscowość", u"miejscowość", ADDRESS_HOME_CITY},
  });

  GeoIpCountryCode kPL = GeoIpCountryCode("PL");
  ParsingContext kPLContext(fields, kPL, LanguageCode("pl"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kPLContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_NAME,
                            /*changed*/ ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
                            ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY));
}

// Test that the actions are not applied since there is apartment related field
// after ADDRESS_HOME_HOUSE_NUMBER (for Poland).
TEST(FormStructureRationalizationEngine, TestPLHouseNumberAndAptNoChange) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Imię", u"n", NAME_FIRST},
      {u"Nachname", u"a", NAME_LAST},
      {u"Ulica", u"Ulica", ADDRESS_HOME_STREET_NAME},
      {u"Nomeru domu", u"nomeru domu", ADDRESS_HOME_HOUSE_NUMBER},
      {u"Nomeru localu", u"nomeru localu", ADDRESS_HOME_APT_NUM},
      {u"KOD", u"kod", ADDRESS_HOME_ZIP},
      {u"Miejscowość", u"miejscowość", ADDRESS_HOME_CITY},
  });

  GeoIpCountryCode kPL = GeoIpCountryCode("PL");
  ParsingContext kPLContext(fields, kPL, LanguageCode("pl"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kPLContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_NAME,
                            ADDRESS_HOME_HOUSE_NUMBER, ADDRESS_HOME_APT_NUM,
                            ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY));
}

// Test that the actions are applied if there is no next field after
// ADDRESS_HOME_HOUSE_NUMBER (for Poland).
TEST(FormStructureRationalizationEngine, TestPLHouseNumberAndAptWithNoNext) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Imię", u"n", NAME_FIRST},
      {u"Nachname", u"a", NAME_LAST},
      {u"Ulica", u"Ulica", ADDRESS_HOME_STREET_NAME},
      {u"Nomeru domu", u"nomeru domu", ADDRESS_HOME_HOUSE_NUMBER},
  });

  GeoIpCountryCode kPL = GeoIpCountryCode("PL");
  ParsingContext kPLContext(fields, kPL, LanguageCode("pl"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kPLContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_NAME,
                            /*changed*/ ADDRESS_HOME_HOUSE_NUMBER_AND_APT));
}

// Verifies that fields classified as ADDRESS_HOME_LINE1 without a following
// ADDRESS_HOME_LINE2 are reclassified as ADDRESS_HOME_STREET_ADDRESS for PL
// forms.
TEST(FormStructureRationalizationEngine, TestPLAddressLine1WithNoNext) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Imię", u"n", NAME_FIRST},
      {u"Nachname", u"a", NAME_LAST},
      {u"Ulica i Nomeru domu", u"ulica i nomeru domu", ADDRESS_HOME_LINE1},
      {u"Kod pocztowy", u"kod pocztowy", ADDRESS_HOME_ZIP},
  });

  GeoIpCountryCode kPL = GeoIpCountryCode("PL");
  ParsingContext kPLContext(fields, kPL, LanguageCode("pl"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kPLContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_ADDRESS,
                            /*changed*/ ADDRESS_HOME_ZIP));
}

// Verifies that fields classified as ADDRESS_HOME_LINE1 with a following
// repeated ADDRESS_HOME_LINE1 are reclassified as ADDRESS_HOME_LINE1 and
// ADDRESS_HOME_LINE2 for IT forms.
TEST(FormStructureRationalizationEngine, TestITAddressLine1WithAL1Next) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Nome", u"nome", NAME_FIRST},
      {u"Cognome", u"cognome", NAME_LAST},
      {u"Indirizzo", u"indirizzo", ADDRESS_HOME_LINE1},
      {u"Indirizzo", u"indirizzo", ADDRESS_HOME_LINE1},
      {u"Codice postale", u"codice postale", ADDRESS_HOME_ZIP},
  });

  GeoIpCountryCode kIT = GeoIpCountryCode("IT");
  ParsingContext kITContext(fields, kIT, LanguageCode("it"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kITContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_LINE1,
                            /*changed*/ ADDRESS_HOME_LINE2, ADDRESS_HOME_ZIP));
}

// Verifies that fields classified as ADDRESS_HOME_LINE1 without a following
// ADDRESS_HOME_LINE2 are reclassified as ADDRESS_HOME_STREET_ADDRESS for IT
// forms.
TEST(FormStructureRationalizationEngine, TestITAddressLine1WithNoNext) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Nome", u"nome", NAME_FIRST},
      {u"Cognome", u"cognome", NAME_LAST},
      {u"Indirizzo", u"indirizzo", ADDRESS_HOME_LINE1},
      {u"Codice postale", u"codice postale", ADDRESS_HOME_ZIP},
  });

  GeoIpCountryCode kIT = GeoIpCountryCode("IT");
  ParsingContext kITContext(fields, kIT, LanguageCode("it"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kITContext, fields, nullptr);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST,
                    /*changed*/ ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_ZIP));
}

// Test that a house number field not followed by an apartment is treated
// as a ADDRESS_HOME_HOUSE_NUMBER_AND_APT in the Netherlands.
TEST(FormStructureRationalizationEngine, TestNLHouseNumberAndAptChanged) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Voornaam", u"voornaam", NAME_FIRST},
      {u"Achternaam", u"achternaam", NAME_LAST},
      {u"Straat", u"straat", ADDRESS_HOME_STREET_NAME},
      {u"Huisnummer", u"huisnummer", ADDRESS_HOME_HOUSE_NUMBER},
      {u"Zipcode", u"zipcode", ADDRESS_HOME_ZIP},
      {u"Plaats", u"plaats", ADDRESS_HOME_CITY},
  });

  GeoIpCountryCode kNL = GeoIpCountryCode("NL");
  ParsingContext kNLContext(fields, kNL, LanguageCode("nl"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kNLContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_NAME,
                            /*changed*/ ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
                            ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY));
}

// Test that the actions are not applied since there is apartment related field
// after ADDRESS_HOME_HOUSE_NUMBER (for the Netherlands).
TEST(FormStructureRationalizationEngine, TestNLHouseNumberAndAptNoChange) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Voornaam", u"voornaam", NAME_FIRST},
      {u"Achternaam", u"achternaam", NAME_LAST},
      {u"Straat", u"straat", ADDRESS_HOME_STREET_NAME},
      {u"Huisnummer", u"huisnummer", ADDRESS_HOME_HOUSE_NUMBER},
      {u"Toevoeging", u"toevoeging", ADDRESS_HOME_APT_NUM},
      {u"Zipcode", u"zipcode", ADDRESS_HOME_ZIP},
      {u"Plaats", u"plaats", ADDRESS_HOME_CITY},
  });

  GeoIpCountryCode kNL = GeoIpCountryCode("NL");
  ParsingContext kNLContext(fields, kNL, LanguageCode("nl"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kNLContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_NAME,
                            ADDRESS_HOME_HOUSE_NUMBER, ADDRESS_HOME_APT_NUM,
                            ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY));
}

// Test that the actions are applied if there is no next field after
// ADDRESS_HOME_HOUSE_NUMBER (for the Netherlands).
TEST(FormStructureRationalizationEngine, TestNLHouseNumberAndAptWithNoNext) {
  base::test::ScopedFeatureList feature_list{
      kTestFeatureForFormStructureRationalizationEngine};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {u"Voornaam", u"voornaam", NAME_FIRST},
      {u"Achternaam", u"achternaam", NAME_LAST},
      {u"Straat", u"straat", ADDRESS_HOME_STREET_NAME},
      {u"Huisnummer", u"huisnummer", ADDRESS_HOME_HOUSE_NUMBER},
  });

  GeoIpCountryCode kNL = GeoIpCountryCode("NL");
  ParsingContext kNLContext(fields, kNL, LanguageCode("nl"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kNLContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_NAME,
                            /*changed*/ ADDRESS_HOME_HOUSE_NUMBER_AND_APT));
}

// Tests that in India, if there is landmark field detected, but there is no
// field for locality. The street address related field is set to
// `ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY`.
TEST(FormStructureRationalizationEngine, TestINStreetLocationWithNoLocality) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kTestFeatureForFormStructureRationalizationEngine,
       features::kAutofillUseINAddressModel},
      {});

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields(
      {{u"First name", u"first-name", NAME_FIRST},
       {u"Last name", u"lastname", NAME_LAST},
       {u"Street Address", u"street-address", ADDRESS_HOME_STREET_LOCATION},
       {u"Landmark", u"landmark", ADDRESS_HOME_LANDMARK},
       {u"City", u"city", ADDRESS_HOME_CITY}});

  GeoIpCountryCode kIN = GeoIpCountryCode("IN");
  ParsingContext kINContext(fields, kIN, LanguageCode("en"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kINContext, fields, nullptr);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST,
                    /*changed*/ ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
                    ADDRESS_HOME_LANDMARK, ADDRESS_HOME_CITY));
}

// Tests that in India, if there is only one street address related field, it is
// set to `ADDRESS_HOME_STREET_ADDRESS`.
TEST(FormStructureRationalizationEngine, TestINAddressLine1WithNoNext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kTestFeatureForFormStructureRationalizationEngine,
       features::kAutofillUseINAddressModel},
      {});

  std::vector<std::unique_ptr<AutofillField>> fields =
      CreateFields({{u"First name", u"first-name", NAME_FIRST},
                    {u"Last name", u"lastname", NAME_LAST},
                    {u"Street Address", u"street-address", ADDRESS_HOME_LINE1},
                    {u"City", u"city", ADDRESS_HOME_CITY}});

  GeoIpCountryCode kIN = GeoIpCountryCode("IN");
  ParsingContext kINContext(fields, kIN, LanguageCode("en"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kINContext, fields, nullptr);

  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST,
                            /*changed*/ ADDRESS_HOME_STREET_ADDRESS,
                            ADDRESS_HOME_CITY));
}

// Tests that in India, if there is locality field detected, but there is no
// field for landmark. The street address related field is set to
// `ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK`.
TEST(FormStructureRationalizationEngine, TestINStreetLocationWithNoLandmark) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kTestFeatureForFormStructureRationalizationEngine,
       features::kAutofillUseINAddressModel},
      {});

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields(
      {{u"First name", u"first-name", NAME_FIRST},
       {u"Last name", u"lastname", NAME_LAST},
       {u"Street Address", u"street-address", ADDRESS_HOME_LINE1},
       {u"Locality", u"L=locality", ADDRESS_HOME_DEPENDENT_LOCALITY},
       {u"City", u"city", ADDRESS_HOME_CITY}});

  GeoIpCountryCode kIN = GeoIpCountryCode("IN");
  ParsingContext kINContext(fields, kIN, LanguageCode("en"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kINContext, fields, nullptr);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST,
                    /*changed*/ ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK,
                    ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY));
}

// Tests that in India, if there is street location field detected, but there is
// no field for landmark. The locality related field is set to
// `ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK`.
TEST(FormStructureRationalizationEngine,
     TestINDependantLocalityWithNoLandmark) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kTestFeatureForFormStructureRationalizationEngine,
       features::kAutofillUseINAddressModel},
      {});

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields(
      {{u"First name", u"first-name", NAME_FIRST},
       {u"Last name", u"lastname", NAME_LAST},
       {u"Street Address", u"street-address", ADDRESS_HOME_STREET_LOCATION},
       {u"Locality", u"locality", ADDRESS_HOME_DEPENDENT_LOCALITY},
       {u"City", u"city", ADDRESS_HOME_CITY}});

  GeoIpCountryCode kIN = GeoIpCountryCode("IN");
  ParsingContext kINContext(fields, kIN, LanguageCode("en"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);
  ApplyRationalizationEngineRules(kINContext, fields, nullptr);

  EXPECT_THAT(
      GetTypes(fields),
      FieldTypesAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_LOCATION,
                    /*changed*/ ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK,
                    ADDRESS_HOME_CITY));
}

// Tests that in Japan, if there are name fields duplicated, the second pair is
// classified as alternative.
TEST(FormStructureRationalizationEngine, TestJPAlternativeNames) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kTestFeatureForFormStructureRationalizationEngine,
       features::kAutofillSupportPhoneticNameForJP},
      {});

  // Most common order of name fields in JP.
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields(
      {{u"Last name", u"lastname", NAME_LAST},
       {u"First name", u"firstname", NAME_FIRST},
       {u"Phonetic last name", u"lastname", NAME_LAST},
       {u"Phonetic given name", u"firstname", NAME_FIRST},
       {u"Street Address", u"street-address", ADDRESS_HOME_STREET_ADDRESS}});

  GeoIpCountryCode kJP = GeoIpCountryCode("JP");
  ParsingContext kJPContext(fields, kJP, LanguageCode("en"), GetPatternFile(),
                            /*active_features=*/{}, /*log_manager=*/nullptr);

  ApplyRationalizationEngineRules(kJPContext, fields, nullptr);
  EXPECT_THAT(GetTypes(fields),
              FieldTypesAre(NAME_LAST, NAME_FIRST,
                            /*changed*/ ALTERNATIVE_FAMILY_NAME,
                            /*changed*/ ALTERNATIVE_GIVEN_NAME,
                            ADDRESS_HOME_STREET_ADDRESS));

  // Check that the inversed order of name fields is also supported.
  std::vector<std::unique_ptr<AutofillField>> given_name_first_fields =
      CreateFields({{u"First name", u"firstname", NAME_FIRST},
                    {u"Last name", u"lastname", NAME_LAST},
                    {u"Phonetic given name", u"firstname", NAME_FIRST},
                    {u"Phonetic last name", u"lastname", NAME_LAST},
                    {u"Street Address", u"street-address",
                     ADDRESS_HOME_STREET_ADDRESS}});

  ApplyRationalizationEngineRules(kJPContext, given_name_first_fields, nullptr);
  EXPECT_THAT(GetTypes(given_name_first_fields),
              FieldTypesAre(NAME_FIRST, NAME_LAST,
                            /*changed*/ ALTERNATIVE_GIVEN_NAME,
                            /*changed*/ ALTERNATIVE_FAMILY_NAME,
                            ADDRESS_HOME_STREET_ADDRESS));
}

}  // namespace
}  // namespace autofill::rationalization
