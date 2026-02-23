// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {
namespace {

using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsSubsetOf;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::ValuesIn;

class AutofillAttributeTypeTest_FieldTypeRelations
    : public testing::TestWithParam<AttributeType> {};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillAttributeTypeTest_FieldTypeRelations,
                         ValuesIn(DenseSet<AttributeType>::all()));

// Tests the co-domain of AttributeType::field_type().
TEST_P(AutofillAttributeTypeTest_FieldTypeRelations, FieldType) {
  AttributeType at = GetParam();
  EXPECT_THAT(at.field_type(), AnyOf(ResultOf(&GroupTypeOfFieldType,
                                              FieldTypeGroup::kAutofillAi),
                                     Eq(NAME_FULL)));
}

// Tests the co-domain of AttributeType::field_subtypes().
TEST_P(AutofillAttributeTypeTest_FieldTypeRelations, FieldSubtypes) {
  AttributeType at = GetParam();
  EXPECT_THAT(
      at.field_subtypes(),
      AnyOf(Each(ResultOf(&GroupTypeOfFieldType, FieldTypeGroup::kAutofillAi)),
            Each(ResultOf(&GroupTypeOfFieldType, FieldTypeGroup::kName))));
  EXPECT_THAT(at.field_subtypes(), Contains(at.field_type()));
}

// Tests the co-domain of AttributeType::storable_field_types().
TEST_P(AutofillAttributeTypeTest_FieldTypeRelations, StorableFieldTypes) {
  AttributeType at = GetParam();
  EXPECT_THAT(test_api(at).storable_field_types(),
              IsSubsetOf(at.field_subtypes()));
  EXPECT_THAT(test_api(at).storable_field_types(), Contains(at.field_type()));
}

TEST(AutofillAttributeTypeTest, Relationships_PassportName) {
  AttributeType a = AttributeType(AttributeTypeName::kPassportName);
  EXPECT_EQ(a.entity_type(), EntityType(EntityTypeName::kPassport));
  EXPECT_THAT(a.field_subtypes(),
              UnorderedElementsAre(
                  NAME_HONORIFIC_PREFIX, NAME_FIRST, NAME_MIDDLE, NAME_LAST,
                  NAME_LAST_PREFIX, NAME_LAST_CORE, NAME_LAST_FIRST,
                  NAME_LAST_SECOND, NAME_LAST_CONJUNCTION, NAME_MIDDLE_INITIAL,
                  NAME_FULL, NAME_SUFFIX, ALTERNATIVE_FAMILY_NAME,
                  ALTERNATIVE_GIVEN_NAME, ALTERNATIVE_FULL_NAME));
}

TEST(AutofillAttributeTypeTest, IsObfuscated) {
  AttributeType name = AttributeType(AttributeTypeName::kPassportName);
  AttributeType number = AttributeType(AttributeTypeName::kPassportNumber);
  EXPECT_FALSE(name.is_obfuscated());
  EXPECT_TRUE(number.is_obfuscated());
}

TEST(AutofillEntityTypeTest, Attributes) {
  using enum AttributeTypeName;
  EntityType e = EntityType(EntityTypeName::kPassport);
  EXPECT_THAT(e.attributes(),
              UnorderedElementsAre(AttributeType(kPassportName),
                                   AttributeType(kPassportNumber),
                                   AttributeType(kPassportCountry),
                                   AttributeType(kPassportExpirationDate),
                                   AttributeType(kPassportIssueDate)));
  ASSERT_FALSE(e.attributes().empty());
}

TEST(AutofillEntityTypeTest, ImportConstraints) {
  using enum AttributeTypeName;
  EntityType e = EntityType(EntityTypeName::kPassport);
  EXPECT_THAT(e.import_constraints(), UnorderedElementsAre(UnorderedElementsAre(
                                          AttributeType(kPassportNumber))));
}

TEST(AutofillEntityTypeTest, MergeConstraints) {
  using enum AttributeTypeName;
  EntityType e = EntityType(EntityTypeName::kPassport);
  EXPECT_THAT(
      e.merge_constraints(),
      ElementsAre(UnorderedElementsAre(AttributeType(kPassportNumber))));
}

TEST(AutofillEntityTypeTest, StrikeKeys) {
  EntityType e = EntityType(EntityTypeName::kPassport);
  EXPECT_THAT(e.strike_keys(), ElementsAre(UnorderedElementsAre(AttributeType(
                                   AttributeTypeName::kPassportNumber))));
}

TEST(AutofillEntityTypeTest, NameAsString) {
  EntityType e = EntityType(EntityTypeName::kPassport);
  AttributeType a = *e.attributes().begin();
  EXPECT_EQ(e.name_as_string(), "Passport");
  EXPECT_EQ(a.name_as_string(), "Name");
}

TEST(AutofillEntityTypeTest, DisambiguationOrder) {
  using enum AttributeTypeName;
  auto lt = [](AttributeTypeName lhs, AttributeTypeName rhs) {
    return AttributeType::DisambiguationOrder(AttributeType(lhs),
                                              AttributeType(rhs));
  };
  EXPECT_TRUE(lt(kPassportName, kPassportCountry));
  EXPECT_TRUE(lt(kPassportCountry, kPassportExpirationDate));
  EXPECT_TRUE(lt(kPassportCountry, kPassportIssueDate));
  EXPECT_TRUE(lt(kPassportCountry, kPassportNumber));
  EXPECT_FALSE(lt(kPassportNumber, kPassportIssueDate));
}

TEST(AutofillEntityTypeTest, Disabled) {
  using enum EntityTypeName;
  EXPECT_TRUE(EntityType(kPassport).enabled());
  EXPECT_TRUE(EntityType(kDriversLicense).enabled());
  EXPECT_TRUE(EntityType(kVehicle).enabled());
}

// Tests that specifying an "excluded geo-ip" disables the entity in countries
// with that geo ip.
TEST(AutofillEntityTypeTest, EnabledWithCountryCode) {
  EntityType e = EntityType(EntityTypeName::kNationalIdCard);
  EXPECT_TRUE(e.enabled(GeoIpCountryCode("US")));
  EXPECT_TRUE(e.enabled(GeoIpCountryCode("DE")));
  EXPECT_FALSE(e.enabled(GeoIpCountryCode("IN")));
}

TEST(AutofillEntityTypeTest, EntityGetNameForI18n) {
  using enum EntityTypeName;
  EntityType a = EntityType(kPassport);
  EntityType b = EntityType(kDriversLicense);
  EXPECT_EQ(a.GetNameForI18n(), u"Passport");
  EXPECT_EQ(b.GetNameForI18n(), u"Driver's license");
}

TEST(AutofillEntityTypeTest, AttributeGetNameForI18n) {
  using enum AttributeTypeName;
  AttributeType a = AttributeType(kPassportCountry);
  AttributeType b = AttributeType(kVehiclePlateNumber);
  AttributeType c = AttributeType(kDriversLicenseExpirationDate);
  EXPECT_EQ(a.GetNameForI18n(), u"Country");
  EXPECT_EQ(b.GetNameForI18n(), u"License plate");
  EXPECT_EQ(c.GetNameForI18n(), u"Expiration date");
}

TEST(AutofillEntityTypeTest, DataType) {
  using enum AttributeTypeName;
  AttributeType a = AttributeType(kPassportName);
  AttributeType b = AttributeType(kPassportCountry);
  AttributeType c = AttributeType(kDriversLicenseIssueDate);
  AttributeType d = AttributeType(kDriversLicenseState);
  AttributeType e = AttributeType(kVehicleMake);
  EXPECT_EQ(a.data_type(), AttributeType::DataType::kName);
  EXPECT_EQ(b.data_type(), AttributeType::DataType::kCountry);
  EXPECT_EQ(c.data_type(), AttributeType::DataType::kDate);
  EXPECT_EQ(d.data_type(), AttributeType::DataType::kState);
  EXPECT_EQ(e.data_type(), AttributeType::DataType::kString);
}

TEST(AutofillEntityTypeTest, ReadOnly) {
  using enum EntityTypeName;
  EXPECT_FALSE(EntityType(kPassport).read_only());
  EXPECT_TRUE(EntityType(kFlightReservation).read_only());
}

// Tests that `EntityType` and `AttributeType` can be used in
// `absl::flat_hash_map`.
TEST(AutofillEntityTypeTest, CanBeUsedInAbslFlatHashMap) {
  absl::flat_hash_map<EntityType, int> entity_type_map;
  auto passport = EntityType(EntityTypeName::kPassport);
  entity_type_map[passport] = 1;
  EXPECT_EQ(entity_type_map[passport], 1);

  absl::flat_hash_map<AttributeType, int> attribute_type_map;
  auto passport_name = AttributeType(AttributeTypeName::kPassportName);
  attribute_type_map[passport_name] = 2;
  EXPECT_EQ(attribute_type_map[passport_name], 2);
}

}  // namespace
}  // namespace autofill
