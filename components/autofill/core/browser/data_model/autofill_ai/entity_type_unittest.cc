// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(AutofillAttributeTypeTest, Relationships) {
  AttributeType a = AttributeType(AttributeTypeName::kPassportName);
  EXPECT_EQ(a.entity_type(), EntityType(EntityTypeName::kPassport));
  EXPECT_EQ(a.field_type(), PASSPORT_NAME_TAG);
  EXPECT_EQ(a, AttributeType::FromFieldType(PASSPORT_NAME_TAG));
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

TEST(AutofillEntityTypeTest, Syncable) {
  using enum EntityTypeName;
  EXPECT_FALSE(EntityType(kPassport).syncable());
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

}  // namespace
}  // namespace autofill
