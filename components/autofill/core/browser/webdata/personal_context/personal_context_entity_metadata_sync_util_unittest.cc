// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_util.h"

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
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

constexpr EntitySuppressionHash kTestHash(0x12345);

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
      CreateEntityDataFromEntitySuppressionSpecifics(specifics);

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
// supported fields.
TEST(PersonalContextEntityMetadataSyncUtilTest,
     TrimAutofillEntitySuppressionSpecificsDataForCaching) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionHash("test-guid-1234", kTestHash,
                                               base::Time::Now(),
                                               /*base_specifics=*/{});
  specifics.mutable_suppression()->set_hash(kTestHash.value());
  EXPECT_EQ(TrimAutofillEntitySuppressionSpecificsDataForCaching(specifics)
                .ByteSizeLong(),
            0u);

  specifics.mutable_confirmation()->set_hash(kTestHash.value());
  EXPECT_EQ(TrimAutofillEntitySuppressionSpecificsDataForCaching(specifics)
                .ByteSizeLong(),
            0u);
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
