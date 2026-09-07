// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/personal_context/entity_suppression_sync_util.h"

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

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

// Tests that converting an `EntitySuppressionEntry` to specifics and back
// preserves all data.
TEST(EntitySuppressionSyncUtilTest, RoundTripConversion) {
  EntitySuppressionEntry original_entry = CreateTestPassportEntry();

  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry("test-guid-1234",
                                                original_entry);
  std::optional<EntitySuppressionEntry> converted_entry =
      CreateEntitySuppressionEntryFromSpecifics(specifics);

  EXPECT_EQ(specifics.guid(), "test-guid-1234");
  EXPECT_TRUE(AreEntitySuppressionSpecificsValid(specifics));
  ASSERT_TRUE(converted_entry.has_value());
  EXPECT_EQ(*converted_entry, original_entry);
}

// Tests that `EntityData` is correctly created from specifics.
TEST(EntitySuppressionSyncUtilTest, CreateEntityData) {
  EntitySuppressionEntry entry = CreateTestPassportEntry();
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry("test-guid-1234", entry);

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromEntitySuppressionSpecifics(specifics);

  ASSERT_NE(entity_data, nullptr);
  EXPECT_EQ(entity_data->name, "test-guid-1234");
  EXPECT_TRUE(entity_data->specifics.has_autofill_entity_suppression());
  EXPECT_EQ(entity_data->specifics.autofill_entity_suppression().guid(),
            "test-guid-1234");
}

// Tests that `EntityData` is correctly created directly from an
// `EntitySuppressionEntry`.
TEST(EntitySuppressionSyncUtilTest,
     CreateEntityDataFromEntitySuppressionEntry) {
  EntitySuppressionEntry entry = CreateTestPassportEntry();

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromEntitySuppressionEntry("test-guid-1234", entry);

  ASSERT_NE(entity_data, nullptr);
  EXPECT_EQ(entity_data->name, "test-guid-1234");
  EXPECT_TRUE(entity_data->specifics.has_autofill_entity_suppression());
  EXPECT_EQ(entity_data->specifics.autofill_entity_suppression().guid(),
            "test-guid-1234");
}

// Tests that specifics with an empty GUID are considered invalid.
TEST(EntitySuppressionSyncUtilTest, InvalidSpecificsEmptyGuid) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry("", CreateTestPassportEntry());

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics missing an entity suppression key are considered
// invalid.
TEST(EntitySuppressionSyncUtilTest, InvalidSpecificsMissingKey) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics with an unknown entity type name are considered invalid.
TEST(EntitySuppressionSyncUtilTest, InvalidSpecificsUnknownEntityType) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry("test-guid-1234",
                                                CreateTestPassportEntry());
  specifics.mutable_entity_suppression_key()->set_entity_type_name(
      "unknown_entity_type");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics containing an unknown attribute type name are considered
// invalid.
TEST(EntitySuppressionSyncUtilTest, InvalidSpecificsUnknownAttributeType) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry("test-guid-1234",
                                                CreateTestPassportEntry());
  specifics.mutable_entity_suppression_key()->mutable_attributes(0)->set_name(
      "unknown_attribute");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics with an invalid SHA-256 hash size are considered
// invalid.
TEST(EntitySuppressionSyncUtilTest, InvalidSpecificsInvalidHashSize) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry("test-guid-1234",
                                                CreateTestPassportEntry());
  specifics.mutable_entity_suppression_key()
      ->mutable_attributes(0)
      ->set_value_hash("short_hash");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics with duplicate attribute types are considered invalid.
TEST(EntitySuppressionSyncUtilTest, InvalidSpecificsDuplicateAttributes) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry("test-guid-1234",
                                                CreateTestPassportEntry());
  sync_pb::EntitySuppressionKey::Attribute* dup_attr =
      specifics.mutable_entity_suppression_key()->add_attributes();
  dup_attr->set_name(specifics.entity_suppression_key().attributes(0).name());
  dup_attr->set_value_hash(
      specifics.entity_suppression_key().attributes(0).value_hash());

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

// Tests that specifics with empty attributes are considered invalid.
TEST(EntitySuppressionSyncUtilTest, InvalidSpecificsEmptyAttributes) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid("test-guid-1234");
  specifics.mutable_entity_suppression_key()->set_entity_type_name("passport");

  EXPECT_FALSE(AreEntitySuppressionSpecificsValid(specifics));
}

}  // namespace

}  // namespace autofill
