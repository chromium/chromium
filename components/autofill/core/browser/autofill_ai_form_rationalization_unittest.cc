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

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Truly;

testing::Matcher<AutofillFieldWithAttributeType> FieldAndType(
    const std::unique_ptr<AutofillField>& field,
    AttributeType type) {
  return AllOf(
      Field(&AutofillFieldWithAttributeType::field,
            Truly([ptr = field.get()](raw_ref<const AutofillField> field) {
              return &*field == ptr;
            })),
      Field(&AutofillFieldWithAttributeType::type, type));
}

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

  size_t rank = 0;
  for (auto& field : fields) {
    field->set_rank(rank++);
  }

  base::flat_map<LocalFrameToken, size_t> frame_tokens;
  for (auto& field : fields) {
    field->set_section(
        Section::FromFieldIdentifier(*fields.front(), frame_tokens));
  }
  return fields;
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

// Tests that a set of fields that do not contain any AutofillAi field does not
// contain relevant entities.
TEST_F(
    AutofillAiFormRationalizationTest,
    GetRelevantEntitiesForFormsAi_ReturnsEmptyWhenNoAutofillAiFieldIsPresent) {
  std::vector<std::unique_ptr<AutofillField>> fields =
      CreateFields({PASSPORT_ISSUING_COUNTRY, ADDRESS_HOME_LINE1,
                    ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3});

  EXPECT_TRUE(GetRelevantEntityTypesForFields(fields).empty());
}

// Tests that a set of fields that match entities required fields has such
// entities as being relevant.
TEST_F(
    AutofillAiFormRationalizationTest,
    GetRelevantEntitiesForFormsAi_ReturnsEntitiesWhenAutofillAiFieldsMatchRequirements) {
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields(
      {VEHICLE_MAKE, VEHICLE_MODEL, NAME_FIRST, NAME_MIDDLE, NAME_LAST,
       DRIVERS_LICENSE_NUMBER, DRIVERS_LICENSE_EXPIRATION_DATE});
  EXPECT_EQ(
      GetRelevantEntityTypesForFields(fields),
      (DenseSet<EntityType>{EntityType(EntityTypeName::kVehicle),
                            EntityType(EntityTypeName::kDriversLicense)}));
}

// Tests that name fields only do not lead to relevant entities even when they
// are party of entities requirements.
TEST_F(AutofillAiFormRationalizationTest,
       GetRelevantEntitiesForFormsAi_ReturnsEmptyWhenOnlyNameFieldsArePresent) {
  std::vector<std::unique_ptr<AutofillField>> fields =
      CreateFields({NAME_FULL});
  EXPECT_TRUE(GetRelevantEntityTypesForFields(fields).empty());
}

// Tests that adjacent vehicle license plate numbers are erased.
TEST_F(AutofillAiFormRationalizationTest, ClearAdjacentVehiclePlateNumbers) {
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields(
      {// 1x: OK.
       VEHICLE_LICENSE_PLATE,
       // Separator.
       VEHICLE_MAKE,
       // 1x: OK.
       VEHICLE_LICENSE_PLATE,
       // Separator.
       VEHICLE_MODEL,
       // 2x: Rationalized.
       VEHICLE_LICENSE_PLATE, VEHICLE_LICENSE_PLATE,
       // Separator.
       VEHICLE_VIN,
       // 3x: Rationalized.
       VEHICLE_LICENSE_PLATE, VEHICLE_LICENSE_PLATE, VEHICLE_LICENSE_PLATE,
       // Separator.
       NAME_FIRST,
       // 2x: Rationalized.
       VEHICLE_LICENSE_PLATE, UNKNOWN_TYPE, VEHICLE_LICENSE_PLATE,
       // Separator.
       NAME_MIDDLE,
       // 2x: Rationalized, .
       VEHICLE_LICENSE_PLATE, UNKNOWN_TYPE, UNKNOWN_TYPE, VEHICLE_LICENSE_PLATE,
       // Separator.
       NAME_LAST,
       // 2x: Rationalized.
       VEHICLE_LICENSE_PLATE, VEHICLE_LICENSE_PLATE});

  using enum AttributeTypeName;
  EXPECT_THAT(
      RationalizeAndDetermineAttributeTypes(
          fields, fields[0]->section(), EntityType(EntityTypeName::kVehicle)),
      ElementsAre(FieldAndType(fields[0], AttributeType(kVehiclePlateNumber)),
                  FieldAndType(fields[1], AttributeType(kVehicleMake)),
                  FieldAndType(fields[2], AttributeType(kVehiclePlateNumber)),
                  FieldAndType(fields[3], AttributeType(kVehicleModel)),
                  FieldAndType(fields[6], AttributeType(kVehicleVin)),
                  FieldAndType(fields[10], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[14], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[15], AttributeType(kVehiclePlateNumber)),
                  FieldAndType(fields[18], AttributeType(kVehiclePlateNumber)),
                  FieldAndType(fields[19], AttributeType(kVehicleOwner))));
}

}  // namespace

}  // namespace autofill
