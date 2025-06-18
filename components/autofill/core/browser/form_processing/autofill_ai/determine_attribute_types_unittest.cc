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
            test::CreateFieldByRole(!fts.empty() ? fts.front() : EMPTY_TYPE));
        field->set_server_predictions(
            base::ToVector(fts, [](const FieldType ft) {
              return test::CreateFieldPrediction(ft);
            }));
        return field;
      });
  AssignSections(fields);
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

// Tests that DetermineAttributeTypes() doesn't crash on empty lists.
TEST(DetermineAttributeTypesTest, ToleratesEmptyList) {
  // base::test::ScopedFeatureList features_{features::kAutofillAiNoTagTypes};
  EXPECT_THAT(DetermineAttributeTypes({}), IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes({}, Section()), IsEmpty());
  EXPECT_THAT(DetermineAttributeTypes({}, Section(), kPassport), IsEmpty());
}

// Tests that DetermineAttributeTypes() processes `*_TAG` correctly if
TEST(DetermineAttributeTypesTest, LegacyBehavior) {
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

  // DetermineAttributeTypes() overload with Section and AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields, fields.front()->section(), kVehicle),
      vehicle_matcher);
  EXPECT_THAT(DetermineAttributeTypes(fields, fields.front()->section(),
                                      kDriversLicense),
              drivers_license_matcher);
  EXPECT_THAT(
      DetermineAttributeTypes(fields, fields.front()->section(), kPassport),
      IsEmpty());

  // DetermineAttributeTypes() overload with Section, without AttributeType.
  EXPECT_THAT(
      DetermineAttributeTypes(fields, fields.front()->section()),
      UnorderedElementsAre(Pair(kVehicle, vehicle_matcher),
                           Pair(kDriversLicense, drivers_license_matcher)));

  // DetermineAttributeTypes() overload without Section and AttributeType.
  EXPECT_THAT(DetermineAttributeTypes(fields),
              UnorderedElementsAre(
                  Pair(fields.front()->section(),
                       UnorderedElementsAre(
                           Pair(kVehicle, vehicle_matcher),
                           Pair(kDriversLicense, drivers_license_matcher)))));
  EXPECT_THAT(DetermineAttributeTypes({}, Section(),
                                      EntityType(EntityTypeName::kPassport)),
              IsEmpty());
}

}  // namespace
}  // namespace autofill
