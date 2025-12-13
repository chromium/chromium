// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table_test_api.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

// Test fixture for synchronous database operations.
class EntityTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.GetPath().AppendASCII("TestWebDatabase"),
                       &encryptor_));
  }

  EntityTable& table() { return table_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::ScopedTempDir temp_dir_;
  const os_crypt_async::Encryptor encryptor_ =
      os_crypt_async::GetTestEncryptorForTesting();
  EntityTable table_;
  WebDatabase db_;
};

// Tests that the entity and attribute tables preserve entity data between write
// and read.
TEST_F(EntityTableTest, BasicWriteThenRead) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(2),
       .use_date = test::kJune2017 - base::Days(7),
       .use_count = 5});
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  // Flight reservation has frecency override set to departure time.
  EntityInstance fr = test::GetFlightReservationEntityInstance({
      .departure_time = test::kJune2017,
  });

  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(dl));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(fr));
  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, dl, fr));
}

// Tests that AddOrUpdateEntityInstance() correctly adds entities with an id
// that's not formatted as GUID.
TEST_F(EntityTableTest, BasicWriteNonGuidFormatId) {
  EntityInstance vr = test::GetVehicleEntityInstanceWithRandomGuid(
      {.guid = "non-guid-format",
       .record_type = EntityInstance::RecordType::kServerWallet});

  ASSERT_TRUE(table().AddOrUpdateEntityInstance(vr));
  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(vr));
}

// Tests that the entity table preserves read only flag between write and read.
TEST_F(EntityTableTest, BasicWriteThenRead_ReadOnlyInstance) {
  EntityInstance pp =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .are_attributes_read_only =
              EntityInstance::AreAttributesReadOnly{true}});

  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));

  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp));
}

// Tests that the entity table preserves custom use_date, use_count, and
// date_modified values between write and read.
TEST_F(EntityTableTest, CustomUseDateUseCountDateModified) {
  base::Time custom_use_date = test::kJune2017 - base::Days(5);
  int custom_use_count = 10;
  base::Time custom_date_modified = test::kJune2017 - base::Days(2);

  EntityInstance pp = test::GetPassportEntityInstance({
      .date_modified = custom_date_modified,
      .use_date = custom_use_date,
      .use_count = custom_use_count,
  });

  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp));
}

// Tests retrieving entity instances by record type.
TEST_F(EntityTableTest, GetEntityInstancesByRecordType) {
  EntityInstance local_pp = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  EntityInstance server_dl = test::GetDriversLicenseEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});

  ASSERT_TRUE(table().AddOrUpdateEntityInstance(local_pp));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(server_dl));
  EXPECT_THAT(table().GetEntityInstances(),
              UnorderedElementsAre(local_pp, server_dl));
  EXPECT_THAT(table().GetEntityInstances(EntityInstance::RecordType::kLocal),
              UnorderedElementsAre(local_pp));
  EXPECT_THAT(
      table().GetEntityInstances(EntityInstance::RecordType::kServerWallet),
      UnorderedElementsAre(server_dl));
}

// Tests updating entity instances.
TEST_F(EntityTableTest, AddOrUpdateEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(3)});
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_THAT(table().GetEntityInstances(), ElementsAre(pp));

  // Updating a non-existing instance adds it.
  EXPECT_TRUE(table().AddOrUpdateEntityInstance(dl));
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, dl));

  pp = test::GetPassportEntityInstance({
      .name = u"Karlsson",
      .date_modified = test::kJune2017 - base::Days(1),
  });
  EXPECT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, dl));
}

// Tests deleting entity instances by record_type.
TEST_F(EntityTableTest, DeleteEntityInstancesByRecordType) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  EntityInstance wallet_vr = test::GetVehicleEntityInstance({
      .guid = "00000000-0000-4000-8000-123000000000",
      .record_type = EntityInstance::RecordType::kServerWallet,
  });
  EntityInstance local_vr = test::GetVehicleEntityInstance({
      .guid = "00000000-0000-4000-8000-456000000000",
      .record_type = EntityInstance::RecordType::kLocal,
  });
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(dl));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(wallet_vr));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(local_vr));
  ASSERT_THAT(table().GetEntityInstances(),
              UnorderedElementsAre(pp, dl, wallet_vr, local_vr));
  // Delete Wallet entity instances.
  EXPECT_TRUE(
      table().DeleteEntityInstances(EntityInstance::RecordType::kServerWallet));
  EXPECT_THAT(table().GetEntityInstances(),
              UnorderedElementsAre(pp, dl, local_vr));
}

// Tests removing individual entity instances.
TEST_F(EntityTableTest, RemoveEntityInstance) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(dl));

  // Removing an element once removes it.
  // Removing it a second time succeeds but has no effect.
  ASSERT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, dl));
  EXPECT_TRUE(table().RemoveEntityInstance(pp.guid()));
  ASSERT_THAT(table().GetEntityInstances(), ElementsAre(dl));
  EXPECT_TRUE(table().RemoveEntityInstance(pp.guid()));
  ASSERT_THAT(table().GetEntityInstances(), ElementsAre(dl));

  // Same for the other element.
  EXPECT_TRUE(table().RemoveEntityInstance(dl.guid()));
  ASSERT_THAT(table().GetEntityInstances(), IsEmpty());
  EXPECT_TRUE(table().RemoveEntityInstance(dl.guid()));
  ASSERT_THAT(table().GetEntityInstances(), IsEmpty());
}

// Tests removing a date range of entity instances.
TEST_F(EntityTableTest, RemoveEntityInstancesModifiedBetween) {
  auto instances =
      std::array{test::GetPassportEntityInstance(
                     {.date_modified = test::kJune2017 - base::Days(11)}),
                 test::GetDriversLicenseEntityInstance(
                     {.date_modified = test::kJune2017 - base::Days(10)})};
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(instances[0]));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(instances[1]));
  ASSERT_THAT(table().GetEntityInstances(),
              UnorderedElementsAreArray(instances));

  // Elements outside of the date range are not affected.
  EXPECT_TRUE(table().RemoveEntityInstancesModifiedBetween(
      instances[0].date_modified() - base::Days(10),
      instances[0].date_modified() - base::Days(1)));
  EXPECT_THAT(table().GetEntityInstances(),
              UnorderedElementsAreArray(instances));

  // Elements outside of the date range are not affected.
  EXPECT_TRUE(table().RemoveEntityInstancesModifiedBetween(
      instances[1].date_modified() + base::Days(1),
      instances[1].date_modified() + base::Days(10)));
  EXPECT_THAT(table().GetEntityInstances(),
              UnorderedElementsAreArray(instances));

  // Elements in the date range are removed.
  EXPECT_TRUE(table().RemoveEntityInstancesModifiedBetween(
      instances[0].date_modified() - base::Days(1),
      instances[1].date_modified() + base::Days(1)));
  EXPECT_THAT(table().GetEntityInstances(), IsEmpty());
}

// Tests that entity instances without any valid attributes are not returned
// from the database.
TEST_F(EntityTableTest, GetEntityInstancesSkipsEmptyInstances) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();
  ASSERT_THAT(table().GetEntityInstances(), IsEmpty());

  EXPECT_TRUE(table().AddOrUpdateEntityInstance(pp));
  EXPECT_TRUE(table().AddOrUpdateEntityInstance(dl));
  EXPECT_THAT(table().GetEntityInstances(), UnorderedElementsAre(pp, dl));

  // Manipulate the attribute instances: changing their type simulates a change
  // of the entity schema.
  sql::Statement attributes_update;
  attributes_update.Assign(test_api(table()).db()->GetUniqueStatement(
      R"(UPDATE autofill_ai_attributes
         SET attribute_type = attribute_type || 'some-garbage-suffix'
         WHERE entity_guid = ?)"));
  attributes_update.BindString(0, *pp.guid());
  ASSERT_TRUE(attributes_update.Run())
      << "The UPDATE failed: " << test_api(table()).db()->GetErrorMessage()
      << " (Check the table and column names in the "
         "UpdateBuilder() call above.)";

  EXPECT_THAT(table().GetEntityInstances(), ElementsAre(dl));
}

// Tests the EntityInstanceExists method.
TEST_F(EntityTableTest, EntityInstanceExists) {
  EntityInstance pp = test::GetPassportEntityInstance();
  EntityInstance dl = test::GetDriversLicenseEntityInstance();

  // Initially, no entity should exist.
  EXPECT_FALSE(table().EntityInstanceExists(pp.guid()));
  EXPECT_FALSE(table().EntityInstanceExists(dl.guid()));

  // After adding an entity, it should exist.
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  EXPECT_TRUE(table().EntityInstanceExists(pp.guid()));
  EXPECT_FALSE(table().EntityInstanceExists(dl.guid()));

  // After adding another entity, both should exist.
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(dl));
  EXPECT_TRUE(table().EntityInstanceExists(pp.guid()));
  EXPECT_TRUE(table().EntityInstanceExists(dl.guid()));

  // After removing an entity, it should no longer exist.
  ASSERT_TRUE(table().RemoveEntityInstance(pp.guid()));
  EXPECT_FALSE(table().EntityInstanceExists(pp.guid()));
  EXPECT_TRUE(table().EntityInstanceExists(dl.guid()));

  // After removing the other entity, neither should exist.
  ASSERT_TRUE(table().RemoveEntityInstance(dl.guid()));
  EXPECT_FALSE(table().EntityInstanceExists(pp.guid()));
  EXPECT_FALSE(table().EntityInstanceExists(dl.guid()));
}

// Tests that metadata info is removed alongside entities.
TEST_F(EntityTableTest, RemoveEntityInstanceRemovesMetadata) {
  EntityInstance pp = test::GetPassportEntityInstance(
      {.date_modified = test::kJune2017 - base::Days(2),
       .use_date = test::kJune2017 - base::Days(7),
       .use_count = 5});
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));
  ASSERT_TRUE(table().EntityInstanceExists(pp.guid()));

  // Verify that metadata exists in the database.
  sql::Statement s;
  s.Assign(test_api(table()).db()->GetUniqueStatement(
      "SELECT count(*) FROM autofill_ai_entities_metadata WHERE entity_guid = "
      "?"));
  s.BindString(0, *pp.guid());
  ASSERT_TRUE(s.Step());
  EXPECT_GT(s.ColumnInt(0), 0);

  // Remove the entity.
  ASSERT_TRUE(table().RemoveEntityInstance(pp.guid()));
  EXPECT_FALSE(table().EntityInstanceExists(pp.guid()));

  // Verify that metadata is also removed.
  s.Assign(test_api(table()).db()->GetUniqueStatement(
      "SELECT count(*) FROM autofill_ai_entities_metadata WHERE entity_guid = "
      "?"));
  s.BindString(0, *pp.guid());
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(s.ColumnInt(0), 0);
}

// Tests that AddOrUpdateEntityMetadata() correctly adds new metadata.
TEST_F(EntityTableTest, AddOrUpdateEntityMetadata_Add) {
  EntityInstance::EntityMetadata metadata{
      .guid = EntityInstance::EntityId("test-guid-1"),
      .date_modified = base::Time::FromTimeT(100),
      .use_count = 1,
      .use_date = base::Time::FromTimeT(50)};

  ASSERT_TRUE(table().AddOrUpdateEntityMetadata(metadata));
  EXPECT_THAT(test_api(table()).GetMetadataEntries(), ElementsAre(metadata));
}

// Tests that AddOrUpdateEntityMetadata() correctly updates existing metadata.
TEST_F(EntityTableTest, AddOrUpdateEntityMetadata_Update) {
  EntityInstance::EntityMetadata metadata{
      .guid = EntityInstance::EntityId("test-guid-2"),
      .date_modified = base::Time::FromTimeT(100),
      .use_count = 1,
      .use_date = base::Time::FromTimeT(50)};
  ASSERT_TRUE(table().AddOrUpdateEntityMetadata(metadata));

  metadata.use_count = 2;
  metadata.date_modified = base::Time::FromTimeT(200);
  metadata.use_date = base::Time::FromTimeT(150);
  ASSERT_TRUE(table().AddOrUpdateEntityMetadata(metadata));
  EXPECT_THAT(test_api(table()).GetMetadataEntries(), ElementsAre(metadata));
}

// Tests that GetSyncedMetadata() returns nothing if only local entities are
// present.
TEST_F(EntityTableTest, GetSyncedMetadata_OnlyLocal) {
  EntityInstance local_pp = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(local_pp));
  EXPECT_THAT(table().GetSyncedMetadata(), IsEmpty());
}

// Tests that GetSyncedMetadata() returns metadata for all entities if only
// server entities are present.
TEST_F(EntityTableTest, GetSyncedMetadata_OnlyServer) {
  EntityInstance server_dl = test::GetDriversLicenseEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance server_vr = test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(server_dl));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(server_vr));

  EXPECT_THAT(table().GetSyncedMetadata(),
              testing::UnorderedElementsAre(
                  testing::Pair(server_dl.guid(), server_dl.metadata()),
                  testing::Pair(server_vr.guid(), server_vr.metadata())));
}

// Tests that GetSyncedMetadata() returns only metadata for server entities when
// both local and server entities are present.
TEST_F(EntityTableTest, GetSyncedMetadata_Mixed) {
  EntityInstance local_pp = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  EntityInstance server_dl = test::GetDriversLicenseEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(local_pp));
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(server_dl));

  EXPECT_THAT(table().GetSyncedMetadata(),
              testing::UnorderedElementsAre(
                  testing::Pair(server_dl.guid(), server_dl.metadata())));
}

// Tests that GetEntityMetadata() returns the correct metadata for an existing
// entity.
TEST_F(EntityTableTest, GetEntityMetadata_ExistingEntity) {
  EntityInstance pp = test::GetPassportEntityInstance();
  ASSERT_TRUE(table().AddOrUpdateEntityInstance(pp));

  std::optional<EntityInstance::EntityMetadata> metadata =
      table().GetEntityMetadata(pp.guid());
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(*metadata, pp.metadata());
}

// Tests that GetEntityMetadata() returns nullopt for a non-existent entity.
TEST_F(EntityTableTest, GetEntityMetadata_NonExistentEntity) {
  EntityInstance::EntityId non_existent_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  std::optional<EntityInstance::EntityMetadata> metadata =
      table().GetEntityMetadata(non_existent_guid);
  EXPECT_EQ(metadata, std::nullopt);
}

}  // namespace
}  // namespace autofill
