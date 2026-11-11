// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_util.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/unknown_field_util.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::EqualsProto;
using ::syncer::test::AddUnknownFieldToProto;
using ::syncer::test::HasUnknownField;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

constexpr EntitySuppressionHash kTestHash(0x12345);
constexpr EntityConfirmationHash kTestConfirmationHash(0x6789A);

AttributeValueHash CreateTestHash(uint8_t fill_value) {
  std::array<uint8_t, crypto::hash::kSha256Size> hash;
  hash.fill(fill_value);
  return AttributeValueHash(hash);
}

EntitySuppressionEntry CreateTestPassportEntry() {
  return EntitySuppressionEntry{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes =
          {
              {AttributeType(AttributeTypeName::kPassportNumber),
               CreateTestHash(0x01)},
          },
  };
}

// Tests that suppression entries are constructed for an entity with a
// non-empty merge constraint.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     SingleConstraintWithNonEmptyAttributes) {
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
TEST(PersonalContextEntityMetadataSyncUtilTest,
     MultipleConstraintsWithNonEmptyAttributes) {
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
TEST(PersonalContextEntityMetadataSyncUtilTest,
     ConstraintWithEmptyAttributesSkipped) {
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
TEST(PersonalContextEntityMetadataSyncUtilTest,
     AllConstraintsEmptyReturnsEmpty) {
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.number = nullptr});

  EXPECT_THAT(GetEntitySuppressionEntries(passport), IsEmpty());
}

// Tests equality comparisons between suppression entries.
TEST(PersonalContextEntityMetadataSyncUtilTest, Equality) {
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
TEST(PersonalContextEntityMetadataSyncUtilTest, GetEntitySuppressionHash) {
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
TEST(PersonalContextEntityMetadataSyncUtilTest,
     GetEntitySuppressionHash_MultipleAttributes) {
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

// Tests that the confirmation hash covers the entity type and all non-empty
// attribute type names and normalized value hashes. Since hashes are synced,
// they must not change.
TEST(PersonalContextEntityMetadataSyncUtilTest, GetEntityConfirmationHash) {
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Jane Doe",
                                  .number = u"P123456",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance drivers_license = test::GetDriversLicenseEntityInstance(
      test::DriversLicenseOptions{.name = u"Jane Doe",
                                  .region = nullptr,
                                  .number = u"P123456",
                                  .expiration_date = nullptr,
                                  .issue_date = nullptr});

  // The leading 20 bits of
  // SHA-256("Passport" + "Name" + SHA-256("jane doe") + "Number" +
  //         SHA-256("p123456")).
  EXPECT_EQ(GetEntityConfirmationHash(passport),
            EntityConfirmationHash(0x42C17));
  // The leading 20 bits of
  // SHA-256("Driver's license" + "Name" + SHA-256("jane doe") + "Number" +
  //         SHA-256("p123456")).
  EXPECT_EQ(GetEntityConfirmationHash(drivers_license),
            EntityConfirmationHash(0x96FC6));
}

// Tests that empty and whitespace-only attributes are skipped when computing
// the confirmation hash.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     GetEntityConfirmationHash_SkipsEmptyAndWhitespaceOnlyAttributes) {
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Jane Doe",
                                  .number = u"P123456",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance passport_with_empty_attributes =
      test::GetPassportEntityInstance(
          test::PassportEntityOptions{.name = u"Jane Doe",
                                      .number = u"P123456",
                                      .country = u"",
                                      .expiry_date = u"",
                                      .issue_date = u"   "});

  EXPECT_NE(GetEntityConfirmationHash(passport), std::nullopt);
  EXPECT_EQ(GetEntityConfirmationHash(passport),
            GetEntityConfirmationHash(passport_with_empty_attributes));
}

// Tests that `GetEntityConfirmationHash()` returns `std::nullopt` if all
// attributes are empty or whitespace-only after normalization.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     GetEntityConfirmationHash_AllAttributesEmptyOrWhitespaceReturnsNullopt) {
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"",
                                  .number = u"   ",
                                  .country = u"\t\n",
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});

  EXPECT_EQ(GetEntityConfirmationHash(passport), std::nullopt);
}

// Tests that confirmation hashing normalizes case and whitespace while still
// distinguishing punctuation, diacritic, and word-boundary differences.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     GetEntityConfirmationHash_NormalizesCaseAndWhitespaceOnly) {
  EntityInstance base_passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Jane Doe",
                                  .number = u"P123456",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance case_and_whitespace_variant = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"  JANE   DOE\t",
                                  .number = u" p123456 ",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance diacritic_variant = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Jané Doe",
                                  .number = u"P123456",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance punctuation_variant = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Jane Doe",
                                  .number = u"P-123456",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance missing_space_variant = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"JaneDoe",
                                  .number = u"P123456",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});

  EXPECT_EQ(GetEntityConfirmationHash(base_passport),
            GetEntityConfirmationHash(case_and_whitespace_variant));
  EXPECT_NE(GetEntityConfirmationHash(base_passport),
            GetEntityConfirmationHash(diacritic_variant));
  EXPECT_NE(GetEntityConfirmationHash(base_passport),
            GetEntityConfirmationHash(punctuation_variant));
  EXPECT_NE(GetEntityConfirmationHash(base_passport),
            GetEntityConfirmationHash(missing_space_variant));
}

// Tests that changing or swapping any attribute value changes the confirmation
// hash.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     GetEntityConfirmationHash_DistinguishesAttributeTypesAndValues) {
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Jane Doe",
                                  .number = u"P123456",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance passport_with_swapped_values = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"P123456",
                                  .number = u"Jane Doe",
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});
  EntityInstance passport_with_country = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Jane Doe",
                                  .number = u"P123456",
                                  .country = u"Sweden",
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});

  EXPECT_NE(GetEntityConfirmationHash(passport),
            GetEntityConfirmationHash(passport_with_swapped_values));
  EXPECT_NE(GetEntityConfirmationHash(passport),
            GetEntityConfirmationHash(passport_with_country));
}

// Tests that converting an `EntitySuppressionHash` to specifics and back
// preserves all data, without syncing full hashes.
TEST(PersonalContextEntityMetadataSyncUtilTest, RoundTripConversion) {
  const base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1234567890));

  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionHash("test-guid-1234", kTestHash,
                                               creation_time,
                                               /*base_specifics=*/{});
  std::optional<EntitySuppressionHash> converted_hash =
      CreateEntitySuppressionHashFromSpecifics(specifics);

  EXPECT_EQ(specifics.guid(), "test-guid-1234");
  EXPECT_EQ(specifics.creation_time_windows_epoch_micros(), 1234567890);
  EXPECT_EQ(specifics.suppression().hash(), kTestHash.value());
  EXPECT_EQ(specifics.entity_suppression_hash(), kTestHash.value());
  EXPECT_FALSE(specifics.has_entity_suppression_key());
  EXPECT_TRUE(AreEntitySuppressionSpecificsValid(specifics));
  EXPECT_EQ(converted_hash, kTestHash);
}

// Tests that specifics with only the `suppression` submessage (without legacy
// `entity_suppression_hash`) are valid and convert to `EntitySuppressionHash`.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     SuppressionSubmessageWithoutLegacyHash) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  specifics.mutable_suppression()->set_hash(kTestHash.value());

  EXPECT_TRUE(AreEntitySuppressionSpecificsValid(specifics));
  EXPECT_EQ(CreateEntitySuppressionHashFromSpecifics(specifics), kTestHash);
}

// Tests that legacy specifics with only `entity_suppression_hash` (without the
// `suppression` submessage) remain valid and convert to
// `EntitySuppressionHash`.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     LegacyEntitySuppressionHashWithoutSuppressionSubmessage) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  specifics.set_entity_suppression_hash(kTestHash.value());

  EXPECT_TRUE(AreEntitySuppressionSpecificsValid(specifics));
  EXPECT_EQ(CreateEntitySuppressionHashFromSpecifics(specifics), kTestHash);
}

// Tests that `MigrateLegacySuppressionSpecificsIfNeeded` populates
// `suppression.hash` when only `entity_suppression_hash` is set.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     MigrateLegacySuppressionSpecificsIfNeeded) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  specifics.set_entity_suppression_hash(kTestHash.value());

  std::optional<sync_pb::AutofillEntitySuppressionSpecifics> migrated =
      MigrateLegacySuppressionSpecificsIfNeeded(specifics);
  ASSERT_TRUE(migrated.has_value());
  EXPECT_EQ(migrated->suppression().hash(), kTestHash.value());
  EXPECT_EQ(migrated->entity_suppression_hash(), kTestHash.value());
  EXPECT_FALSE(
      MigrateLegacySuppressionSpecificsIfNeeded(*migrated).has_value());
}

// Tests that specifics with a `suppression` submessage missing `hash` are
// considered invalid.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     InvalidSpecificsEmptySuppressionSubmessage) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  specifics.mutable_suppression();

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that converting an `EntityConfirmationHash` to specifics and back
// preserves all data, and does not populate suppression fields.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     ConfirmationRoundTripConversion) {
  const base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1234567890));

  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntityConfirmationHash(
          "test-guid-1234", kTestConfirmationHash, creation_time,
          /*base_specifics=*/{});

  EXPECT_EQ(specifics.guid(), "test-guid-1234");
  EXPECT_EQ(specifics.creation_time_windows_epoch_micros(), 1234567890);
  EXPECT_EQ(specifics.confirmation().hash(), kTestConfirmationHash.value());
  EXPECT_FALSE(specifics.has_suppression());
  EXPECT_FALSE(specifics.has_entity_suppression_hash());
  EXPECT_FALSE(specifics.has_entity_suppression_key());
  EXPECT_TRUE(AreEntityConfirmationSpecificsValid(specifics));
  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
  EXPECT_EQ(CreateEntityConfirmationHashFromSpecifics(specifics),
            kTestConfirmationHash);
  EXPECT_EQ(CreateEntitySuppressionHashFromSpecifics(specifics), std::nullopt);
  EXPECT_EQ(MigrateLegacySuppressionSpecificsIfNeeded(specifics), std::nullopt);
}

// Tests that specifics with a `confirmation` submessage missing `hash` are
// considered invalid.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     InvalidSpecificsEmptyConfirmationSubmessage) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  specifics.mutable_confirmation();

  EXPECT_FALSE(AreEntityConfirmationSpecificsValid(specifics));
}

// Tests that `EntityData` is correctly created directly from an
// `EntityConfirmationHash`.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     CreateEntityDataFromEntityConfirmationHash) {
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromEntityConfirmationHash(
          "test-guid-1234", kTestConfirmationHash, base::Time::Now(),
          /*base_specifics=*/{});

  ASSERT_NE(entity_data, nullptr);
  EXPECT_EQ(entity_data->name, "test-guid-1234");
  EXPECT_TRUE(entity_data->specifics.has_autofill_entity_suppression());
  EXPECT_EQ(entity_data->specifics.autofill_entity_suppression().guid(),
            "test-guid-1234");
  EXPECT_EQ(entity_data->specifics.autofill_entity_suppression()
                .confirmation()
                .hash(),
            kTestConfirmationHash.value());
}

// Tests that `CreateSpecificsFromEntitySuppressionHash` preserves unknown
// fields from `base_specifics`.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     CreateSpecificsFromEntitySuppressionHash_MergesBaseSpecifics) {
  sync_pb::AutofillEntitySuppressionSpecifics base_specifics;
  AddUnknownFieldToProto(base_specifics, "unknown_field");
  AddUnknownFieldToProto(*base_specifics.mutable_entity_suppression_key(),
                         "unknown_key_field");

  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionHash(
          "test-guid-1234", kTestHash, base::Time::Now(), base_specifics);

  EXPECT_EQ(specifics.guid(), "test-guid-1234");
  EXPECT_THAT(specifics, HasUnknownField("unknown_field"));
  EXPECT_THAT(specifics.entity_suppression_key(),
              HasUnknownField("unknown_key_field"));
}

// Tests that `EntityData` is correctly created from specifics.
TEST(PersonalContextEntityMetadataSyncUtilTest, CreateEntityData) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionHash("test-guid-1234", kTestHash,
                                               base::Time::Now(),
                                               /*base_specifics=*/{});

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromSpecifics(specifics);

  ASSERT_NE(entity_data, nullptr);
  EXPECT_EQ(entity_data->name, "test-guid-1234");
  EXPECT_TRUE(entity_data->specifics.has_autofill_entity_suppression());
  EXPECT_EQ(entity_data->specifics.autofill_entity_suppression().guid(),
            "test-guid-1234");
}

// Tests that `EntityData` is correctly created directly from an
// `EntitySuppressionHash`.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     CreateEntityDataFromEntitySuppressionHash) {
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromEntitySuppressionHash("test-guid-1234", kTestHash,
                                                base::Time::Now(),
                                                /*base_specifics=*/{});

  ASSERT_NE(entity_data, nullptr);
  EXPECT_EQ(entity_data->name, "test-guid-1234");
  EXPECT_TRUE(entity_data->specifics.has_autofill_entity_suppression());
  EXPECT_EQ(entity_data->specifics.autofill_entity_suppression().guid(),
            "test-guid-1234");
}

// Tests that specifics with an empty GUID are considered invalid.
TEST(PersonalContextEntityMetadataSyncUtilTest, InvalidSpecificsEmptyGuid) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionHash("", kTestHash, base::Time::Now(),
                                               /*base_specifics=*/{});

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics missing an entity suppression key are considered
// invalid.
TEST(PersonalContextEntityMetadataSyncUtilTest, InvalidSpecificsMissingKey) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that `TrimAutofillEntitySuppressionSpecificsDataForCaching` clears all
// supported fields while preserving unknown fields on both suppression and
// confirmation specifics.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     TrimAutofillEntitySuppressionSpecificsDataForCaching) {
  sync_pb::AutofillEntitySuppressionSpecifics suppression_specifics =
      CreateSpecificsFromEntitySuppressionHash("test-guid-1234", kTestHash,
                                               base::Time::Now(),
                                               /*base_specifics=*/{});
  EXPECT_EQ(TrimAutofillEntitySuppressionSpecificsDataForCaching(
                suppression_specifics)
                .ByteSizeLong(),
            0u);

  sync_pb::AutofillEntitySuppressionSpecifics confirmation_specifics =
      CreateSpecificsFromEntityConfirmationHash(
          "test-guid-1234", kTestConfirmationHash, base::Time::Now(),
          /*base_specifics=*/{});
  EXPECT_EQ(TrimAutofillEntitySuppressionSpecificsDataForCaching(
                confirmation_specifics)
                .ByteSizeLong(),
            0u);

  AddUnknownFieldToProto(*confirmation_specifics.mutable_confirmation(),
                         "unknown_confirmation_field");
  sync_pb::AutofillEntitySuppressionSpecifics trimmed_confirmation =
      TrimAutofillEntitySuppressionSpecificsDataForCaching(
          confirmation_specifics);
  EXPECT_FALSE(trimmed_confirmation.confirmation().has_hash());
  EXPECT_THAT(trimmed_confirmation.confirmation(),
              HasUnknownField("unknown_confirmation_field"));
}

// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
sync_pb::AutofillEntitySuppressionSpecifics
CreateLegacyTestPassportSpecifics() {
  const EntitySuppressionEntry entry = CreateTestPassportEntry();
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  sync_pb::EntitySuppressionKey* key =
      specifics.mutable_entity_suppression_key();
  key->set_entity_type_name(std::string(entry.type.name_as_string()));
  for (const auto& [attr_type, hash] : entry.attribute_hashes) {
    sync_pb::EntitySuppressionKey::Attribute* attr = key->add_attributes();
    attr->set_name(std::string(attr_type.name_as_string()));
    attr->set_value_hash(hash.value().data(), hash.value().size());
  }
  return specifics;
}

// Tests that legacy `entity_suppression_key` specifics convert to the same
// `EntitySuppressionHash` as `GetEntitySuppressionHash()` and migrate in-place
// while preserving `entity_suppression_key`.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     LegacyEntitySuppressionKeyConversionAndMigration) {
  const sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateLegacyTestPassportSpecifics();
  const EntitySuppressionHash expected_hash =
      GetEntitySuppressionHash(CreateTestPassportEntry());

  EXPECT_FALSE(specifics.has_suppression());
  EXPECT_FALSE(specifics.has_entity_suppression_hash());
  EXPECT_TRUE(AreEntitySuppressionSpecificsValid(specifics));
  EXPECT_EQ(CreateEntitySuppressionHashFromSpecifics(specifics), expected_hash);

  std::optional<sync_pb::AutofillEntitySuppressionSpecifics> migrated =
      MigrateLegacySuppressionSpecificsIfNeeded(specifics);
  ASSERT_TRUE(migrated.has_value());
  EXPECT_EQ(migrated->suppression().hash(), expected_hash.value());
  EXPECT_EQ(migrated->entity_suppression_hash(), expected_hash.value());
  EXPECT_THAT(migrated->entity_suppression_key(),
              EqualsProto(specifics.entity_suppression_key()));
  EXPECT_FALSE(
      MigrateLegacySuppressionSpecificsIfNeeded(*migrated).has_value());
}

// Tests that specifics with an unknown entity type name are considered invalid.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     InvalidSpecificsUnknownEntityType) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateLegacyTestPassportSpecifics();
  specifics.mutable_entity_suppression_key()->set_entity_type_name(
      "unknown_entity_type");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics containing an unknown attribute type name are considered
// invalid.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     InvalidSpecificsUnknownAttributeType) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateLegacyTestPassportSpecifics();
  specifics.mutable_entity_suppression_key()->mutable_attributes(0)->set_name(
      "unknown_attribute");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics with an invalid SHA-256 hash size are considered
// invalid.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     InvalidSpecificsInvalidHashSize) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateLegacyTestPassportSpecifics();
  specifics.mutable_entity_suppression_key()
      ->mutable_attributes(0)
      ->set_value_hash("short_hash");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics with duplicate attribute types are considered invalid.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     InvalidSpecificsDuplicateAttributes) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateLegacyTestPassportSpecifics();
  sync_pb::EntitySuppressionKey::Attribute* dup_attr =
      specifics.mutable_entity_suppression_key()->add_attributes();
  dup_attr->set_name(specifics.entity_suppression_key().attributes(0).name());
  dup_attr->set_value_hash(
      specifics.entity_suppression_key().attributes(0).value_hash());

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics with empty attributes are considered invalid.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     InvalidSpecificsEmptyAttributes) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  specifics.mutable_entity_suppression_key()->set_entity_type_name("passport");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

}  // namespace

}  // namespace autofill
