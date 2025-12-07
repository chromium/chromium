// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(AutofillAttributeTypeTest, Relationships) {
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

// Tests that specifying an "excluded geo-ip" disabled the entity in countries
// with that geo ip.
TEST(AutofillEntityTypeTest, Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiNationalIdCard);
  EntityType e = EntityType(EntityTypeName::kNationalIdCard);
  EXPECT_FALSE(e.enabled());
  EXPECT_FALSE(e.enabled(GeoIpCountryCode("US")));
}

// Tests that specifying an "excluded geo-ip" disabled the entity in countries
// with that geo ip.
TEST(AutofillEntityTypeTest, EnabledWithCountryCode) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiNationalIdCard};
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

}  // namespace
}  // namespace autofill
