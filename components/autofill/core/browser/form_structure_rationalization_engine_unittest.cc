// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalization_engine.h"

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

}  // namespace
}  // namespace autofill::rationalization
