// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"

#include <vector>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

// Tests that suppression entries are constructed for an entity with a
// non-empty merge constraint.
TEST(EntitySuppressionEntryTest, SingleConstraintWithNonEmptyAttributes) {
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.number = u"P123456"});

  EXPECT_THAT(GetEntitySuppressionEntries(passport),
              ElementsAre(EntitySuppressionEntry{
                  .type = passport.type(),
                  .attribute_hashes =
                      {{AttributeType(AttributeTypeName::kPassportNumber),
                        AttributeValueHash(crypto::hash::Sha256("P123456"))}},
              }));
}

// Tests that suppression entries are constructed for each merge constraint
// whose attributes are non-empty.
TEST(EntitySuppressionEntryTest, MultipleConstraintsWithNonEmptyAttributes) {
  EntityInstance vehicle = test::GetVehicleEntityInstance(test::VehicleOptions{
      .plate = u"ABC-123",
      .number = u"1HGCR2F83HA000000",
  });

  EXPECT_THAT(
      GetEntitySuppressionEntries(vehicle),
      UnorderedElementsAre(
          EntitySuppressionEntry{
              .type = vehicle.type(),
              .attribute_hashes =
                  {{AttributeType(AttributeTypeName::kVehiclePlateNumber),
                    AttributeValueHash(crypto::hash::Sha256("ABC-123"))}},
          },
          EntitySuppressionEntry{
              .type = vehicle.type(),
              .attribute_hashes = {{AttributeType(
                                        AttributeTypeName::kVehicleVin),
                                    AttributeValueHash(crypto::hash::Sha256(
                                        "1HGCR2F83HA000000"))}},
          }));
}

// Tests that merge constraints with missing/empty attributes are skipped.
TEST(EntitySuppressionEntryTest, ConstraintWithEmptyAttributesSkipped) {
  EntityInstance vehicle = test::GetVehicleEntityInstance(test::VehicleOptions{
      .plate = u"ABC-123",
      .number = nullptr,
  });

  EXPECT_THAT(GetEntitySuppressionEntries(vehicle),
              ElementsAre(EntitySuppressionEntry{
                  .type = vehicle.type(),
                  .attribute_hashes =
                      {{AttributeType(AttributeTypeName::kVehiclePlateNumber),
                        AttributeValueHash(crypto::hash::Sha256("ABC-123"))}},
              }));
}

// Tests that an entity with all merge constraint attributes empty produces no
// suppression entries.
TEST(EntitySuppressionEntryTest, AllConstraintsEmptyReturnsEmpty) {
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.number = nullptr});

  EXPECT_THAT(GetEntitySuppressionEntries(passport), IsEmpty());
}

// Tests equality comparisons between suppression entries.
TEST(EntitySuppressionEntryTest, Equality) {
  EntitySuppressionEntry passport_a{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes = {{AttributeType(AttributeTypeName::kPassportNumber),
                            AttributeValueHash(crypto::hash::Sha256("A"))}},
  };
  EntitySuppressionEntry passport_a_duplicate{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes = {{AttributeType(AttributeTypeName::kPassportNumber),
                            AttributeValueHash(crypto::hash::Sha256("A"))}},
  };
  EntitySuppressionEntry passport_b{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes = {{AttributeType(AttributeTypeName::kPassportNumber),
                            AttributeValueHash(crypto::hash::Sha256("B"))}},
  };
  EntitySuppressionEntry drivers_license_a{
      .type = EntityType(EntityTypeName::kDriversLicense),
      .attribute_hashes = {{AttributeType(
                                AttributeTypeName::kDriversLicenseNumber),
                            AttributeValueHash(crypto::hash::Sha256("A"))}},
  };

  EXPECT_EQ(passport_a, passport_a_duplicate);
  EXPECT_NE(passport_a, passport_b);
  EXPECT_NE(passport_a, drivers_license_a);
}

// Tests that the hash covers the entity type and the attribute value hashes.
// Since hashes are synced, they must not change.
TEST(EntitySuppressionEntryTest, GetEntitySuppressionHash) {
  const AttributeValueHash value_hash(crypto::hash::Sha256("P123456"));
  EntitySuppressionEntry passport{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes = {{AttributeType(AttributeTypeName::kPassportNumber),
                            value_hash}},
  };
  EntitySuppressionEntry drivers_license{
      .type = EntityType(EntityTypeName::kDriversLicense),
      .attribute_hashes = {{AttributeType(
                                AttributeTypeName::kDriversLicenseNumber),
                            value_hash}},
  };

  // The leading 20 bits of SHA-256("Passport" + SHA-256("P123456")).
  EXPECT_EQ(GetEntitySuppressionHash(passport), EntitySuppressionHash(0xCB8CB));
  // The leading 20 bits of SHA-256("Driver's license" + SHA-256("P123456")).
  EXPECT_EQ(GetEntitySuppressionHash(drivers_license),
            EntitySuppressionHash(0xD1384));
}

// Tests that the hash of an entry with multiple attributes covers their value
// hashes in ascending order, independently of the attribute types.
TEST(EntitySuppressionEntryTest, GetEntitySuppressionHash_MultipleAttributes) {
  const AttributeType name(AttributeTypeName::kPassportName);
  const AttributeType number(AttributeTypeName::kPassportNumber);
  const AttributeValueHash jane_doe(crypto::hash::Sha256("Jane Doe"));
  const AttributeValueHash p123456(crypto::hash::Sha256("P123456"));
  EntitySuppressionEntry passport{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes = {{name, jane_doe}, {number, p123456}},
  };
  EntitySuppressionEntry passport_with_swapped_values{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes = {{name, p123456}, {number, jane_doe}},
  };

  // The leading 20 bits of
  // SHA-256("Passport" + SHA-256("Jane Doe") + SHA-256("P123456")).
  EXPECT_EQ(GetEntitySuppressionHash(passport), EntitySuppressionHash(0xBF075));
  EXPECT_EQ(GetEntitySuppressionHash(passport_with_swapped_values),
            EntitySuppressionHash(0xBF075));
}

}  // namespace
}  // namespace autofill
