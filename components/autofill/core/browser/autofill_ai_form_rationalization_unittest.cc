// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/dense_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::Field;

std::string GetEntityAttributesStringRepresentation(
    DenseSet<AttributeType> attributes) {
  return base::JoinString(
      base::ToVector(attributes, &AttributeType::name_as_string), ", ");
}

std::vector<std::unique_ptr<AutofillField>> CreateFields(
    const std::vector<FieldType>& field_types) {
  std::vector<std::unique_ptr<AutofillField>> fields =
      base::ToVector(field_types, [](FieldType type) {
        auto field = std::make_unique<AutofillField>(
            test::GetFormFieldData(test::FieldDescription({.role = type})));
        field->set_server_predictions({test::CreateFieldPrediction(type)});
        return field;
      });

  base::flat_map<LocalFrameToken, size_t> frame_tokens;
  for (auto& field : fields) {
    field->set_section(
        Section::FromFieldIdentifier(*fields.front(), frame_tokens));
  }
  return fields;
}

std::vector<std::unique_ptr<AutofillField>> CreateFields(
    const DenseSet<AttributeType>& attribute_types) {
  return CreateFields(
      base::ToVector(attribute_types, &AttributeType::field_type));
}
auto IsAutofillFieldWithType(
    const AutofillFieldWithAttributeType& autofill_field_with_type) {
  return AllOf(Field(&AutofillFieldWithAttributeType::field,
                     autofill_field_with_type.field),
               Field(&AutofillFieldWithAttributeType::type,
                     autofill_field_with_type.type));
}

template <typename Matcher>
auto AreAutofillFieldsWithTypes(const std::vector<Matcher>& matchers) {
  return ElementsAreArray(base::ToVector(matchers, IsAutofillFieldWithType));
}

class AutofillAiFormRationalizationTest : public testing::Test {
 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
};

// For each entity type, this test checks that if a set of attribute types match
// at least one of the required fields set in an entity definition,
// the form will not be rationalized, which means it will be considered an
// AutofillAi form. Conversely, it also tests the opposite.
//
// For example, for a passport the required fields may be {number} and
// {expiry date}. Then the following forms would be considered AutofillAi
// forms: {number}, {expiry date}, {number, name}, but {name, country} would
// not.
TEST_F(AutofillAiFormRationalizationTest, EntitiesAreRationalized) {
  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    // Check that forms that match the constraints are not rationalized away.
    for (DenseSet<AttributeType> required_fields :
         entity_type.required_fields()) {
      SCOPED_TRACE(testing::Message()
                   << "entity=" << entity_type << " "
                   << "required fields="
                   << GetEntityAttributesStringRepresentation(required_fields));
      // Note that the input is matching exactly a certain
      // `required_fields`.
      std::vector<AutofillFieldWithAttributeType> input;
      for (AttributeType type : required_fields) {
        input.emplace_back(AutofillField(), type);
      }
      std::vector<AutofillFieldWithAttributeType> output =
          RationalizeAttributeTypesForTesting(input, entity_type);
      EXPECT_THAT(input, AreAutofillFieldsWithTypes(output))
          << "Expected fields to match entity requirements and therefore no "
             "rationalization to happen";
    }

    // Check that forms that match none of the requirements are rationalized.
    // `attributes_that_do_not_match_requirements` is initialized to all
    // attributes of an entity, then all required fields are removed.
    DenseSet<AttributeType> attributes_that_do_not_match_requirements =
        entity_type.attributes();
    for (DenseSet<AttributeType> required_fields :
         entity_type.required_fields()) {
      attributes_that_do_not_match_requirements.erase_all(required_fields);
    }
    std::vector<AutofillFieldWithAttributeType> input;
    for (AttributeType type : attributes_that_do_not_match_requirements) {
      input.emplace_back(AutofillField(), type);
    }
    SCOPED_TRACE(testing::Message()
                 << "entity=" << entity_type << " "
                 << "fields="
                 << GetEntityAttributesStringRepresentation(
                        attributes_that_do_not_match_requirements));
    std::vector<AutofillFieldWithAttributeType> output =
        RationalizeAttributeTypesForTesting(input, entity_type);
    EXPECT_EQ(output.size(), 0u)
        << "Rationalization failed for entity " << entity_type
        << ". The fields in the input do not contain any of the entity's "
           "required field sets. The rationalization should have excluded all "
           "fields.";
  }
}

// Tests that a set of fields that do not contain any AutofillAi field is not
// relevant for AutofillAi.
TEST_F(
    AutofillAiFormRationalizationTest,
    AreFieldsRelevantForAutofillAi_ReturnsFalseWhenNoAutofillAiFieldIsPresent) {
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields(
      {NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3});

  EXPECT_FALSE(AreFieldsRelevantForAutofillAi(fields));
}

// Tests that a set of fields that is not a superset of some of the EntityType's
// required fields set is not relevant for AutofillAi, even if some of these
// fields are AutofillAi fields.
TEST_F(
    AutofillAiFormRationalizationTest,
    AreFieldsRelevantForAutofillAi_ReturnsFalseWhenRequiredAutofillAiFieldsAreNotPresent) {
  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    if (!entity_type.enabled()) {
      continue;
    }
    // Check that a set of fields that do not match any
    // entity requirement is not relevant for AutofillAi.
    DenseSet<AttributeType> attributes_that_do_not_match_requirements =
        entity_type.attributes();
    for (DenseSet<AttributeType> required_fields :
         entity_type.required_fields()) {
      attributes_that_do_not_match_requirements.erase_all(required_fields);
    }
    SCOPED_TRACE(testing::Message()
                 << "entity=" << entity_type << ", "
                 << "required fields="
                 << GetEntityAttributesStringRepresentation(
                        attributes_that_do_not_match_requirements));
    EXPECT_FALSE(AreFieldsRelevantForAutofillAi(
        CreateFields(attributes_that_do_not_match_requirements)))
        << "Expected fields not to be relevant for AutofillAi as they do not "
           "match any requirement";
  }
}

// Tests that a set of fields that match an entity required fields are
// relevant for AutofillAi.
TEST_F(
    AutofillAiFormRationalizationTest,
    AreFieldsRelevantForAutofillAi_ReturnsTrueWhenRequiredAutofillAiFieldsArePresent) {
  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    if (!entity_type.enabled()) {
      continue;
    }
    // Check that if a set of required fields is present, the fields are
    // relevant for AutofillAi. Note that there is an exception if a set
    // contains only name fields, as they are required but not sufficient
    // (AutofillAi requires more than a single name field, regardless of the
    // entity).
    for (DenseSet<AttributeType> rt : entity_type.required_fields()) {
      SCOPED_TRACE(testing::Message()
                   << "entity=" << entity_type << " "
                   << "required fields="
                   << GetEntityAttributesStringRepresentation(rt));
      if (rt.size() == 1 &&
          (*rt.begin()).data_type() == AttributeType::DataType::kName) {
        EXPECT_FALSE(AreFieldsRelevantForAutofillAi(CreateFields(rt)))
            << "Expected fields not to be relevant for AutofillAi since its a "
               "single name field";
      } else {
        EXPECT_TRUE(AreFieldsRelevantForAutofillAi(CreateFields(rt)))
            << "Expected fields to be relevant for AutofillAi";
      }
    }
  }
}

}  // namespace

}  // namespace autofill
