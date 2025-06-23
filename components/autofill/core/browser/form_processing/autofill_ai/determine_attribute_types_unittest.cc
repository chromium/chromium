// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"

#include <memory>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

void PrintTo(const AutofillFieldWithAttributeType& f, ::std::ostream* os) {
  *os << f.field->global_id() << " -> " << f.type.name_as_string();
}

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

constexpr auto kVehicle = EntityType(EntityTypeName::kVehicle);
constexpr auto kDriversLicense = EntityType(EntityTypeName::kDriversLicense);
constexpr auto kPassport = EntityType(EntityTypeName::kPassport);

std::vector<std::unique_ptr<AutofillField>> CreateFields(
    const std::vector<std::vector<FieldType>>& field_types) {
  std::vector<std::unique_ptr<AutofillField>> fields =
      base::ToVector(field_types, [](const std::vector<FieldType>& fts) {
        auto field = std::make_unique<AutofillField>(
            test::GetFormFieldData(test::FieldDescription(
                {.role = !fts.empty() ? fts.front() : EMPTY_TYPE})));
        field->set_server_predictions(
            base::ToVector(fts, [](const FieldType ft) {
              return test::CreateFieldPrediction(ft);
            }));
        return field;
      });

  base::flat_map<LocalFrameToken, size_t> frame_tokens;
  for (auto& field : fields) {
    field->set_section(
        Section::FromFieldIdentifier(*fields.front(), frame_tokens));
  }
  return fields;
}

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

class DetermineAttributeTypesTest : public testing::Test {
 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
};

// Tests that DetermineAttributeTypes() doesn't crash on empty lists.
TEST_F(DetermineAttributeTypesTest, ToleratesEmptyList) {
  EXPECT_THAT(DetermineAttributeTypes({}), IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes({}, Section()), IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes({}, Section(), kPassport), IsEmpty());
}

// Tests that DetermineAttributeTypes() is empty on forms that have no entities.
TEST_F(DetermineAttributeTypesTest, IsEmptyInUnrelatedForm) {
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {NAME_FULL},
      {ADDRESS_HOME_LINE1},
      {ADDRESS_HOME_LINE2},
      {ADDRESS_HOME_LINE3},
      {ADDRESS_HOME_ZIP},
      {ADDRESS_HOME_CITY},
      {ADDRESS_HOME_STATE},
      {ADDRESS_HOME_COUNTRY},
  });

  EXPECT_THAT(DetermineAttributeTypes(fields), IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes(fields, Section()), IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes(fields, Section(), kPassport), IsEmpty());
}

// Tests that DetermineAttributeTypes() processes `*_TAG` correctly if
TEST_F(DetermineAttributeTypesTest, LegacyBehavior) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiNoTagTypes);

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {VEHICLE_MAKE},
      {VEHICLE_MODEL},
      {NAME_FIRST, VEHICLE_OWNER_TAG},
      {NAME_MIDDLE},
      {NAME_LAST, DRIVERS_LICENSE_NAME_TAG},
      {DRIVERS_LICENSE_NUMBER},
      {DRIVERS_LICENSE_EXPIRATION_DATE},
  });

  using enum AttributeTypeName;
  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[0], AttributeType(kVehicleMake)),
                  FieldAndType(fields[1], AttributeType(kVehicleModel)),
                  FieldAndType(fields[2], AttributeType(kVehicleOwner)));
  auto drivers_license_matcher = ElementsAre(
      FieldAndType(fields[4], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[5], AttributeType(kDriversLicenseNumber)),
      FieldAndType(fields[6], AttributeType(kDriversLicenseExpirationDate)));
  const Section section = fields.front()->section();

  // DetermineAttributeTypes() overload with Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kVehicle),
              vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kDriversLicense),
              drivers_license_matcher);

  // DetermineAttributeTypes() overload with Section, without EntityType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields, section),
      UnorderedElementsAre(Pair(kVehicle, vehicle_matcher),
                           Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields),
      UnorderedElementsAre(
          Pair(section, UnorderedElementsAre(
                            Pair(kVehicle, vehicle_matcher),
                            Pair(kDriversLicense, drivers_license_matcher)))));
}

// Tests that DetermineAttributeTypes() assigns static types correctly.
TEST_F(DetermineAttributeTypesTest, AssignsStaticTypes) {
  base::test::ScopedFeatureList feature_list{features::kAutofillAiNoTagTypes};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {DRIVERS_LICENSE_NUMBER},
      {VEHICLE_MAKE},
      {VEHICLE_MODEL},
      {DRIVERS_LICENSE_EXPIRATION_DATE},
      {ADDRESS_HOME_ZIP},
  });

  using enum AttributeTypeName;
  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[1], AttributeType(kVehicleMake)),
                  FieldAndType(fields[2], AttributeType(kVehicleModel)));
  auto drivers_license_matcher = ElementsAre(
      FieldAndType(fields[0], AttributeType(kDriversLicenseNumber)),
      FieldAndType(fields[3], AttributeType(kDriversLicenseExpirationDate)));
  const Section section = fields.front()->section();

  // DetermineAttributeTypes() overload with Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kVehicle),
              vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kDriversLicense),
              drivers_license_matcher);

  // DetermineAttributeTypes() overload with Section, without EntityType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields, section),
      UnorderedElementsAre(Pair(kVehicle, vehicle_matcher),
                           Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields),
      UnorderedElementsAre(
          Pair(section, UnorderedElementsAre(
                            Pair(kVehicle, vehicle_matcher),
                            Pair(kDriversLicense, drivers_license_matcher)))));
}

// Tests that DetermineAttributeTypes() assigns dynamic types correctly:
// - It must look at both the forward and backward vicinity.
// - Fields can have multiple types simultaneously.
TEST_F(DetermineAttributeTypesTest, AssignsDynamicTypesToTheVicinity) {
  base::test::ScopedFeatureList features_list{features::kAutofillAiNoTagTypes};

  // The NAME_{FIRST,MIDDLE,LAST} fields are expected to be assigned to both the
  // vehicle and the driver's license entities.
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {VEHICLE_MAKE},
      {VEHICLE_MODEL},
      {NAME_FIRST},
      {NAME_MIDDLE},
      {NAME_LAST},
      {DRIVERS_LICENSE_NUMBER},
      {DRIVERS_LICENSE_EXPIRATION_DATE},
  });

  using enum AttributeTypeName;
  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[0], AttributeType(kVehicleMake)),
                  FieldAndType(fields[1], AttributeType(kVehicleModel)),
                  FieldAndType(fields[2], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[3], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[4], AttributeType(kVehicleOwner)));
  auto drivers_license_matcher = ElementsAre(
      FieldAndType(fields[2], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[3], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[4], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[5], AttributeType(kDriversLicenseNumber)),
      FieldAndType(fields[6], AttributeType(kDriversLicenseExpirationDate)));
  const Section section = fields.front()->section();

  // DetermineAttributeTypes() overload with Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kVehicle),
              vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kDriversLicense),
              drivers_license_matcher);

  // DetermineAttributeTypes() overload with Section, without EntityType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields, section),
      UnorderedElementsAre(Pair(kVehicle, vehicle_matcher),
                           Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields),
      UnorderedElementsAre(
          Pair(section, UnorderedElementsAre(
                            Pair(kVehicle, vehicle_matcher),
                            Pair(kDriversLicense, drivers_license_matcher)))));
}

// Tests that DetermineAttributeTypes() propagates dynamic types forward.
TEST_F(DetermineAttributeTypesTest, PropagatesDynamicTypesForward) {
  base::test::ScopedFeatureList features_list{features::kAutofillAiNoTagTypes};
  using enum AttributeTypeName;

  // The last NAME_FULL field is too far away to be reached by the propagation.
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {VEHICLE_MAKE}, {UNKNOWN_TYPE}, {NAME_FULL},    {UNKNOWN_TYPE},
      {UNKNOWN_TYPE}, {NAME_FULL},    {UNKNOWN_TYPE}, {UNKNOWN_TYPE},
      {UNKNOWN_TYPE}, {NAME_FULL},    {UNKNOWN_TYPE}, {UNKNOWN_TYPE},
      {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {NAME_FULL},    {UNKNOWN_TYPE},
      {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {UNKNOWN_TYPE},
      {NAME_FULL},
  });

  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[0], AttributeType(kVehicleMake)),
                  FieldAndType(fields[2], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[5], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[9], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[14], AttributeType(kVehicleOwner)));
  const Section section = fields.front()->section();
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kVehicle),
              vehicle_matcher);
}

// Tests that DetermineAttributeTypes() propagates dynamic types backward.
TEST_F(DetermineAttributeTypesTest, PropagatesDynamicTypesBackward) {
  base::test::ScopedFeatureList features_list{features::kAutofillAiNoTagTypes};
  using enum AttributeTypeName;

  // The last NAME_FULL field is too far away to be reached by the propagation.
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {NAME_FULL},    {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {UNKNOWN_TYPE},
      {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {NAME_FULL},    {UNKNOWN_TYPE},
      {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {NAME_FULL},
      {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {NAME_FULL},
      {UNKNOWN_TYPE}, {UNKNOWN_TYPE}, {NAME_FULL},    {UNKNOWN_TYPE},
      {VEHICLE_MAKE},
  });

  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[6], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[11], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[15], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[18], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[20], AttributeType(kVehicleMake)));
  const Section section = fields.front()->section();
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kVehicle),
              vehicle_matcher);
}

// Tests that DetermineAttributeTypes() propagates dynamic types even if there
// are other entities between the source and target.
TEST_F(DetermineAttributeTypesTest,
       PropagatesDynamicTypesForwardAcrossEntities) {
  base::test::ScopedFeatureList features_list{features::kAutofillAiNoTagTypes};
  using enum AttributeTypeName;

  std::vector<std::unique_ptr<AutofillField>> fields =
      CreateFields({{DRIVERS_LICENSE_NUMBER}, {VEHICLE_VIN}, {NAME_FULL}});

  auto drivers_license_matcher =
      ElementsAre(FieldAndType(fields[0], AttributeType(kDriversLicenseNumber)),
                  FieldAndType(fields[2], AttributeType(kDriversLicenseName)));
  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[1], AttributeType(kVehicleVin)),
                  FieldAndType(fields[2], AttributeType(kVehicleOwner)));
  const Section section = fields.front()->section();

  // DetermineAttributeTypes() overload with Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kVehicle),
              vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kDriversLicense),
              drivers_license_matcher);

  // DetermineAttributeTypes() overload with Section, without EntityType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields, section),
      UnorderedElementsAre(Pair(kVehicle, vehicle_matcher),
                           Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields),
      UnorderedElementsAre(
          Pair(section, UnorderedElementsAre(
                            Pair(kVehicle, vehicle_matcher),
                            Pair(kDriversLicense, drivers_license_matcher)))));
}

// Tests that DetermineAttributeTypes() propagates dynamic types even if there
// are other entities between the source and target.
TEST_F(DetermineAttributeTypesTest,
       PropagatesDynamicTypesBackwardAcrossEntities) {
  base::test::ScopedFeatureList features_list{features::kAutofillAiNoTagTypes};
  using enum AttributeTypeName;

  std::vector<std::unique_ptr<AutofillField>> fields =
      CreateFields({{NAME_FULL}, {DRIVERS_LICENSE_NUMBER}, {VEHICLE_VIN}});

  auto drivers_license_matcher = ElementsAre(
      FieldAndType(fields[0], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[1], AttributeType(kDriversLicenseNumber)));
  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[0], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[2], AttributeType(kVehicleVin)));
  const Section section = fields.front()->section();

  // DetermineAttributeTypes() overload with Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kVehicle),
              vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, section, kDriversLicense),
              drivers_license_matcher);

  // DetermineAttributeTypes() overload with Section, without EntityType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields, section),
      UnorderedElementsAre(Pair(kVehicle, vehicle_matcher),
                           Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields),
      UnorderedElementsAre(
          Pair(section, UnorderedElementsAre(
                            Pair(kVehicle, vehicle_matcher),
                            Pair(kDriversLicense, drivers_license_matcher)))));
}

// Tests that DetermineAttributeTypes() isolates fields from different sections
// from another.
TEST_F(DetermineAttributeTypesTest, DistinguishesBetweenSections) {
  base::test::ScopedFeatureList features_list{features::kAutofillAiNoTagTypes};

  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {NAME_FIRST},
      {NAME_MIDDLE},
      {NAME_LAST},
      {VEHICLE_MAKE},
      {VEHICLE_MODEL},
      {NAME_FIRST},
      {NAME_MIDDLE},
      {NAME_LAST},
      {DRIVERS_LICENSE_NUMBER},
      {DRIVERS_LICENSE_EXPIRATION_DATE},
  });
  AssignSections(fields);

  using enum AttributeTypeName;
  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[0], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[1], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[2], AttributeType(kVehicleOwner)),
                  FieldAndType(fields[3], AttributeType(kVehicleMake)),
                  FieldAndType(fields[4], AttributeType(kVehicleModel)));
  auto drivers_license_matcher = ElementsAre(
      FieldAndType(fields[5], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[6], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[7], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[8], AttributeType(kDriversLicenseNumber)),
      FieldAndType(fields[9], AttributeType(kDriversLicenseExpirationDate)));
  const Section vehicle_section = fields[0]->section();
  const Section drivers_license_section = fields[5]->section();
  ASSERT_NE(vehicle_section, drivers_license_section);

  // DetermineAttributeTypes() overload with Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields, vehicle_section, kVehicle),
              vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, vehicle_section, kDriversLicense),
              IsEmpty());
  EXPECT_THAT(
      DetermineAttributeTypes(fields, drivers_license_section, kDriversLicense),
      drivers_license_matcher);
  EXPECT_THAT(
      DetermineAttributeTypes(fields, drivers_license_section, kVehicle),
      IsEmpty());

  // DetermineAttributeTypes() overload with Section, without EntityType.
  EXPECT_THAT(DetermineAttributeTypes(fields, vehicle_section),
              UnorderedElementsAre(Pair(kVehicle, vehicle_matcher)));
  EXPECT_THAT(
      DetermineAttributeTypes(fields, drivers_license_section),
      UnorderedElementsAre(Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields),
      UnorderedElementsAre(
          Pair(vehicle_section, ElementsAre(Pair(kVehicle, vehicle_matcher))),
          Pair(drivers_license_section,
               ElementsAre(Pair(kDriversLicense, drivers_license_matcher)))));
}

// Tests for that the overloads behave equivalently:
// - `DetermineAttributeTypes(fields, section, entity)`
// - `DetermineAttributeTypes(fields, section)[entity]`
// - `DetermineAttributeTypes(fields)[section][entity]`
TEST_F(DetermineAttributeTypesTest, OverloadEquivalence) {
  base::test::ScopedFeatureList features_list{features::kAutofillAiNoTagTypes};

  // We create four fields such that
  // - `field[0]` propagates to `field[2]` and
  // - `field[3]` propagates to `field[1]`.
  // In particular, one propagation should not block the other.
  std::vector<std::unique_ptr<AutofillField>> fields = CreateFields({
      {VEHICLE_MAKE},
      {NAME_FULL},
      {NAME_FULL},
      {DRIVERS_LICENSE_NUMBER},
  });
  base::flat_map<LocalFrameToken, size_t> frame_tokens;
  Section section1 = Section::FromFieldIdentifier(*fields[0], frame_tokens);
  Section section2 = Section::FromFieldIdentifier(*fields[2], frame_tokens);
  ASSERT_NE(section1, section2);
  fields[0]->set_section(section1);
  fields[1]->set_section(section2);
  fields[2]->set_section(section1);
  fields[3]->set_section(section2);

  using enum AttributeTypeName;
  auto vehicle_matcher =
      ElementsAre(FieldAndType(fields[0], AttributeType(kVehicleMake)),
                  FieldAndType(fields[2], AttributeType(kVehicleOwner)));
  auto drivers_license_matcher = ElementsAre(
      FieldAndType(fields[1], AttributeType(kDriversLicenseName)),
      FieldAndType(fields[3], AttributeType(kDriversLicenseNumber)));

  EXPECT_THAT(DetermineAttributeTypes(fields, section1, kVehicle),
              vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, section2, kVehicle), IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes(fields, section1, kDriversLicense),
              IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes(fields, section2, kDriversLicense),
              drivers_license_matcher);

  // DetermineAttributeTypes() overload with Section, without EntityType.
  EXPECT_THAT(DetermineAttributeTypes(fields, section1),
              ElementsAre(Pair(kVehicle, vehicle_matcher)));
  EXPECT_THAT(DetermineAttributeTypes(fields, section2),
              ElementsAre(Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields),
              UnorderedElementsAre(
                  Pair(section1, ElementsAre(Pair(kVehicle, vehicle_matcher))),
                  Pair(section2, ElementsAre(Pair(kDriversLicense,
                                                  drivers_license_matcher)))));
}

}  // namespace
}  // namespace autofill
