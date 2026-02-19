// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_metadata_sync_bridge.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table_test_api.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_test_utils.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/unknown_field_util.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::test::EqualsProto;
using syncer::test::AddUnknownFieldToProto;
using syncer::test::HasUnknownField;
using test::GetPassportEntityInstance;
using test::GetVehicleEntityInstance;
using test::MaskEntityInstance;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;
using testing::Return;
using testing::ReturnRef;
using testing::SizeIs;
using testing::UnorderedElementsAre;

syncer::EntityData SpecificsToEntity(
    const sync_pb::AutofillValuableMetadataSpecifics& metadata_specifics) {
  syncer::EntityData entity_data;
  entity_data.name = metadata_specifics.valuable_id();
  sync_pb::AutofillValuableMetadataSpecifics* specifics =
      entity_data.specifics.mutable_autofill_valuable_metadata();
  specifics->CopyFrom(metadata_specifics);
  return entity_data;
}

EntityInstance::EntityMetadata test_metadata() {
  return EntityInstance::EntityMetadata{
      .guid = EntityInstance::EntityId("some_id"),
      .date_modified = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(13347400000000000u)),
      .use_count = 5,
      .use_date = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(13379000000000000u))};
}
std::vector<EntityInstance::EntityMetadata>
ExtractEntitiesMetadataFromDataBatch(std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<EntityInstance::EntityMetadata> entities;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    entities.push_back(CreateEntityMetadataFromSpecifics(
        data_pair.second->specifics.autofill_valuable_metadata()));
  }
  return entities;
}

EntityInstance CreateServerVehicleEntityInstance(
    test::VehicleOptions options = {}) {
  options.nickname = "";
  options.record_type = EntityInstance::RecordType::kServerWallet;
  return test::GetVehicleEntityInstance(options);
}


class ValuableMetadataSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&sync_metadata_table_);
    db_.AddTable(&entity_table_);
    db_.AddTable(&valuables_table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"),
             &encryptor_);
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));
    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));

    bridge_ = std::make_unique<ValuableMetadataSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  std::vector<EntityInstance::EntityMetadata> GetEntityMetadataEntries() {
    return test_api(entity_table_).GetMetadataEntries();
  }

  std::vector<ValuableMetadata> GetValuableMetadataEntries() {
    std::vector<ValuableMetadata> all_metadata;
    for (const auto& [guid, metadata] :
         valuables_table_.GetAllValuableMetadata()) {
      all_metadata.push_back(metadata);
    }
    return all_metadata;
  }

  ValuableMetadataSyncBridge& bridge() { return *bridge_; }

  testing::NiceMock<MockAutofillWebDataBackend>& backend() { return backend_; }

  EntityTable& entity_table() { return entity_table_; }

  ValuablesTable& valuables_table() { return valuables_table_; }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  const os_crypt_async::Encryptor encryptor_ =
      os_crypt_async::GetTestEncryptorForTesting();
  EntityTable entity_table_;
  ValuablesTable valuables_table_;
  AutofillSyncMetadataTable sync_metadata_table_;
  WebDatabase db_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<ValuableMetadataSyncBridge> bridge_;
  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncAutofillValuableMetadata};
};

// Tests that IsEntityDataValid() returns true for valid entity data.
TEST_F(ValuableMetadataSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData entity_data;
  sync_pb::AutofillValuableMetadataSpecifics* specifics =
      entity_data.specifics.mutable_autofill_valuable_metadata();
  specifics->set_valuable_id("some_id");
  EXPECT_TRUE(bridge().IsEntityDataValid(entity_data));
}

// Tests that IsEntityDataValid() returns false for entity data with an empty
// valuable_id.
TEST_F(ValuableMetadataSyncBridgeTest, IsEntityDataValid_EmptyValuableId) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_autofill_valuable_metadata();
  EXPECT_FALSE(bridge().IsEntityDataValid(entity_data));
}

// Tests that GetClientTag() and GetStorageKey() return the correct valuable_id.
TEST_F(ValuableMetadataSyncBridgeTest, GetClientTagAndStorageKey) {
  syncer::EntityData entity_data;
  sync_pb::AutofillValuableMetadataSpecifics* specifics =
      entity_data.specifics.mutable_autofill_valuable_metadata();
  specifics->set_valuable_id("some_id");

  EXPECT_EQ(bridge().GetClientTag(entity_data), "some_id");
  EXPECT_EQ(bridge().GetStorageKey(entity_data), "some_id");
}

// Tests that TrimAllSupportedFieldsFromRemoteSpecifics() correctly trims
// all supported fields from the specifics.
TEST_F(ValuableMetadataSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::AutofillValuableMetadataSpecifics* metadata =
      entity_specifics.mutable_autofill_valuable_metadata();
  metadata->set_valuable_id("some_id");
  metadata->set_use_count(5);
  metadata->set_last_used_date_unix_epoch_micros(12345);
  metadata->set_last_modified_date_unix_epoch_micros(12345);
  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics)
                .ByteSizeLong(),
            0u);
}

// Test that MergeFullSyncData() correctly merges remote data when there is no
// local data.
TEST_F(ValuableMetadataSyncBridgeTest, MergeFullSyncData_NoLocalData) {
  syncer::EntityChangeList entity_change_list;
  const EntityInstance::EntityMetadata metadata = test_metadata();
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *metadata.guid,
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(

          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_VALUABLE_METADATA));

  EXPECT_FALSE(bridge()
                   .MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                      std::move(entity_change_list))
                   .has_value());

  EXPECT_THAT(GetEntityMetadataEntries(), UnorderedElementsAre(metadata));
}

// Test that MergeFullSyncData() correctly merges remote data when local data
// is a subset of remote data.
TEST_F(ValuableMetadataSyncBridgeTest,
       MergeFullSyncData_LocalDataSubsetOfServerData) {
  const EntityInstance vehicle1 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000"});
  entity_table().AddOrUpdateEntityInstance(vehicle1);

  syncer::EntityChangeList entity_change_list;
  const EntityInstance vehicle2 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *vehicle1.guid(),
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(
          vehicle1.metadata(),
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *vehicle2.guid(),
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(
          vehicle2.metadata(),
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));

  // No data is uploaded to the server.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_VALUABLE_METADATA));

  EXPECT_FALSE(bridge()
                   .MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                      std::move(entity_change_list))
                   .has_value());

  EXPECT_THAT(GetEntityMetadataEntries(),
              UnorderedElementsAre(vehicle1.metadata(), vehicle2.metadata()));
}

// Test that MergeFullSyncData() correctly merges remote data and uploads
// local-only data.
TEST_F(ValuableMetadataSyncBridgeTest,
       MergeFullSyncData_LocalDataSupersetOfServerData) {
  const EntityInstance vehicle1 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000"});
  const EntityInstance vehicle2 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  entity_table().AddOrUpdateEntityInstance(vehicle1);
  entity_table().AddOrUpdateEntityInstance(vehicle2);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *vehicle1.guid(),
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(
          vehicle1.metadata(),
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));

  EXPECT_CALL(mock_processor(), Put(*vehicle2.guid(), _, _));
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_VALUABLE_METADATA));

  EXPECT_FALSE(bridge()
                   .MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                      std::move(entity_change_list))
                   .has_value());

  EXPECT_THAT(GetEntityMetadataEntries(),
              UnorderedElementsAre(vehicle1.metadata(), vehicle2.metadata()));
}

// Test that MergeFullSyncData() ignores the local data without a `PassType`.
TEST_F(ValuableMetadataSyncBridgeTest,
       MergeFullSyncData_IgnoresLocalDataWithoutPassType) {
  // Orders are not supported by the bridge.
  entity_table().AddOrUpdateEntityInstance(
      MaskEntityInstance(test::GetOrderEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet})));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_VALUABLE_METADATA));

  EXPECT_FALSE(bridge()
                   .MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                      syncer::EntityChangeList())
                   .has_value());
}

// Test that supported fields and nested messages are successfully trimmed but
// that unsupported fields are preserved.
TEST_F(ValuableMetadataSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics_PreserveUnsupportedFields) {
  sync_pb::EntitySpecifics trimmed_entity_specifics;
  sync_pb::AutofillValuableMetadataSpecifics*
      valuable_metadata_specifics_with_only_unknown_fields =
          trimmed_entity_specifics.mutable_autofill_valuable_metadata();

  // Set an unsupported field in the top-level message.
  *valuable_metadata_specifics_with_only_unknown_fields
       ->mutable_unknown_fields() = "unsupported_fields";

  // Create a copy and set a value to the same nested message that already
  // contains an unsupported field.
  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_autofill_valuable_metadata() =
      *valuable_metadata_specifics_with_only_unknown_fields;
  entity_specifics.mutable_autofill_valuable_metadata()->set_valuable_id(
      "some_id");

  EXPECT_THAT(
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics),
      EqualsProto(trimmed_entity_specifics));
}

// Tests that ApplyIncrementalSyncChanges() correctly adds a new metadata item.
TEST_F(ValuableMetadataSyncBridgeTest, ApplyIncrementalSyncChanges_Add) {
  syncer::EntityChangeList entity_change_list;
  const EntityInstance::EntityMetadata metadata = test_metadata();
  sync_pb::AutofillValuableMetadataSpecifics specifics =
      CreateSpecificsFromEntityMetadata(
          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{});
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *metadata.guid, SpecificsToEntity(specifics)));

  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_VALUABLE_METADATA));

  EXPECT_FALSE(
      bridge()
          .ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(entity_change_list))
          .has_value());

  EXPECT_THAT(GetEntityMetadataEntries(), UnorderedElementsAre(metadata));
}

// Tests that ApplyIncrementalSyncChanges() correctly updates an existing
// metadata item.
TEST_F(ValuableMetadataSyncBridgeTest, ApplyIncrementalSyncChanges_Update) {
  // Add an initial metadata item.
  syncer::EntityChangeList add_changes;
  EntityInstance::EntityMetadata metadata = test_metadata();
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      *metadata.guid,
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(

          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  // Now, update it.
  syncer::EntityChangeList update_changes;
  metadata.use_count = 10;
  metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13315000000000000u));
  update_changes.push_back(syncer::EntityChange::CreateUpdate(
      *metadata.guid,
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(

          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));

  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_VALUABLE_METADATA));

  EXPECT_FALSE(
      bridge()
          .ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(update_changes))
          .has_value());

  EXPECT_THAT(GetEntityMetadataEntries(), UnorderedElementsAre(metadata));
}

// Tests that ApplyIncrementalSyncChanges() ignores deletions.
TEST_F(ValuableMetadataSyncBridgeTest, ApplyIncrementalSyncChanges_Delete) {
  // Add an initial metadata item.
  syncer::EntityChangeList add_changes;
  EntityInstance::EntityMetadata metadata = test_metadata();
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      *metadata.guid,
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(

          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  ASSERT_THAT(GetEntityMetadataEntries(), SizeIs(1));

  // Now, delete it.
  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete(
      *metadata.guid,
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(

          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));

  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_VALUABLE_METADATA));

  EXPECT_FALSE(
      bridge()
          .ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(delete_changes))
          .has_value());

  // The metadata should still be there.
  EXPECT_THAT(GetEntityMetadataEntries(), SizeIs(1));
}

// Tests that GetAllData() returns all metadata entries from the database.
TEST_F(ValuableMetadataSyncBridgeTest, GetAllData) {
  const EntityInstance vehicle1 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000",
       .date_modified = base::Time::FromSecondsSinceUnixEpoch(100),
       .use_date = base::Time::FromSecondsSinceUnixEpoch(100),
       .use_count = 2});
  const EntityInstance vehicle2 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000",
       .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
       .use_date = base::Time::FromSecondsSinceUnixEpoch(500),
       .use_count = 7});
  entity_table().AddOrUpdateEntityInstance(vehicle1);
  entity_table().AddOrUpdateEntityInstance(vehicle2);

  std::unique_ptr<syncer::DataBatch> batch = bridge().GetAllDataForDebugging();
  ASSERT_TRUE(batch);
  EXPECT_THAT(ExtractEntitiesMetadataFromDataBatch(std::move(batch)),
              UnorderedElementsAre(vehicle1.metadata(), vehicle2.metadata()));
}

// Tests that GetAllData() supports metadata entries for private passes.
TEST_F(ValuableMetadataSyncBridgeTest,
       GetAllData_SupportsMetadataForPrivatePasses) {
  const EntityInstance passport =
      MaskEntityInstance(test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
  const EntityInstance driver_license =
      MaskEntityInstance(test::GetDriversLicenseEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
  const EntityInstance redress_numer =
      MaskEntityInstance(test::GetRedressNumberEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
  const EntityInstance ktn =
      MaskEntityInstance(test::GetKnownTravelerNumberInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
  const EntityInstance national_id =
      MaskEntityInstance(test::GetNationalIdCardEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet}));

  entity_table().AddOrUpdateEntityInstance(passport);
  entity_table().AddOrUpdateEntityInstance(driver_license);
  entity_table().AddOrUpdateEntityInstance(redress_numer);
  entity_table().AddOrUpdateEntityInstance(ktn);
  entity_table().AddOrUpdateEntityInstance(national_id);

  std::unique_ptr<syncer::DataBatch> batch = bridge().GetAllDataForDebugging();
  ASSERT_TRUE(batch);
  EXPECT_THAT(
      ExtractEntitiesMetadataFromDataBatch(std::move(batch)),
      UnorderedElementsAre(passport.metadata(), driver_license.metadata(),
                           redress_numer.metadata(), ktn.metadata(),
                           national_id.metadata()));
}

// Tests that GetAllData() ignores metadata entries without a `PassType`.
TEST_F(ValuableMetadataSyncBridgeTest,
       GetAllData_IgnoresMetadataWithoutPassType) {
  // Orders are not supported by the bridge.
  entity_table().AddOrUpdateEntityInstance(test::GetOrderEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet}));

  std::unique_ptr<syncer::DataBatch> batch = bridge().GetAllDataForDebugging();
  ASSERT_TRUE(batch);
  EXPECT_FALSE(batch->HasNext());
}

// Tests that GetDataForCommit() returns the specified metadata entries.
TEST_F(ValuableMetadataSyncBridgeTest, GetDataForCommit) {
  const EntityInstance vehicle1 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000",
       .date_modified = base::Time::FromSecondsSinceUnixEpoch(100),
       .use_date = base::Time::FromSecondsSinceUnixEpoch(100),
       .use_count = 2});
  const EntityInstance vehicle2 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000",
       .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
       .use_date = base::Time::FromSecondsSinceUnixEpoch(500),
       .use_count = 7});
  entity_table().AddOrUpdateEntityInstance(vehicle1);
  entity_table().AddOrUpdateEntityInstance(vehicle2);

  std::unique_ptr<syncer::DataBatch> batch =
      bridge().GetDataForCommit({"00000000-0000-4000-8000-300000000000"});

  ASSERT_TRUE(batch);
  EXPECT_THAT(ExtractEntitiesMetadataFromDataBatch(std::move(batch)),
              UnorderedElementsAre(vehicle2.metadata()));
}

// Tests that GetDataForCommit() includes unknown fields from the server.
TEST_F(ValuableMetadataSyncBridgeTest, GetDataForCommit_UnknownFields) {
  const EntityInstance vehicle = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000"});
  entity_table().AddOrUpdateEntityInstance(vehicle);

  sync_pb::EntitySpecifics base_specifics;
  AddUnknownFieldToProto(*base_specifics.mutable_autofill_valuable_metadata(),
                         "unknown_field");

  ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics)
      .WillByDefault(ReturnRef(base_specifics));

  std::unique_ptr<syncer::DataBatch> batch =
      bridge().GetDataForCommit({vehicle.guid().value()});

  ASSERT_TRUE(batch);
  ASSERT_TRUE(batch->HasNext());
  const syncer::KeyAndData& data_pair = batch->Next();
  ASSERT_EQ(data_pair.first, vehicle.guid().value());
  EXPECT_THAT(data_pair.second->specifics.autofill_valuable_metadata(),
              HasUnknownField("unknown_field"));
}

// Tests that ApplyDisableSyncChanges() clears all the `EntityMetadata`.
TEST_F(ValuableMetadataSyncBridgeTest,
       ApplyDisableSyncChanges_ClearsEntityMetadata) {
  const EntityInstance server_vehicle1 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000",
       .use_date = base::Time::FromSecondsSinceUnixEpoch(100),
       .use_count = 2});
  const EntityInstance server_vehicle2 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000",
       .use_date = base::Time::FromSecondsSinceUnixEpoch(500),
       .use_count = 7});
  const EntityInstance local_vehicle2 = test::GetVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-500000000000",
       .use_date = base::Time::FromSecondsSinceUnixEpoch(600),
       .use_count = 9});
  entity_table().AddOrUpdateEntityInstance(server_vehicle1);
  entity_table().AddOrUpdateEntityInstance(server_vehicle2);
  entity_table().AddOrUpdateEntityInstance(local_vehicle2);

  ASSERT_THAT(GetEntityMetadataEntries(),
              UnorderedElementsAre(server_vehicle1.metadata(),
                                   server_vehicle2.metadata(),
                                   local_vehicle2.metadata()));

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  EXPECT_THAT(GetEntityMetadataEntries(),
              ElementsAre(local_vehicle2.metadata()));
}

// Tests that ApplyDisableSyncChanges() clears all the `ValuableMetadata`.
TEST_F(ValuableMetadataSyncBridgeTest,
       ApplyDisableSyncChanges_ClearsValuableMetadata) {
  const LoyaltyCard loyalty_card1 = TestLoyaltyCard("1");
  const LoyaltyCard loyalty_card2 = TestLoyaltyCard("2");
  valuables_table().AddOrUpdateLoyaltyCard(loyalty_card1);
  valuables_table().AddOrUpdateLoyaltyCard(loyalty_card2);

  ASSERT_THAT(
      GetValuableMetadataEntries(),
      UnorderedElementsAre(loyalty_card1.metadata(), loyalty_card2.metadata()));

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  EXPECT_THAT(GetValuableMetadataEntries(), IsEmpty());
}

// Tests that `ServerEntityInstanceMetadataChanged()` handles ADD and UPDATE
// changes.
TEST_F(ValuableMetadataSyncBridgeTest,
       ServerEntityInstanceMetadataChanged_AddUpdate) {
  ON_CALL(mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
  const EntityInstance vehicle = CreateServerVehicleEntityInstance();
  entity_table().AddOrUpdateEntityInstance(vehicle);
  ASSERT_THAT(GetEntityMetadataEntries(), ElementsAre(vehicle.metadata()));

  EXPECT_CALL(mock_processor(), Put(*vehicle.guid(), _, _));
  bridge().ServerEntityInstanceMetadataChanged(EntityInstanceMetadataChange(
      EntityInstanceMetadataChange::ADD, vehicle.guid(), vehicle.metadata()));

  EXPECT_CALL(mock_processor(), Put(*vehicle.guid(), _, _));
  bridge().ServerEntityInstanceMetadataChanged(
      EntityInstanceMetadataChange(EntityInstanceMetadataChange::UPDATE,
                                   vehicle.guid(), vehicle.metadata()));
}

// Tests that `ServerEntityInstanceMetadataChanged()` ignores metadata entries
// without a `PassType`.
TEST_F(
    ValuableMetadataSyncBridgeTest,
    ServerEntityInstanceMetadataChanged_AddUpdate_IgnoresMetadataWithoutPassType) {
  ON_CALL(mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
  // Order are not supported by the bridge.
  const EntityInstance order_number =
      MaskEntityInstance(test::GetOrderEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet}));
  entity_table().AddOrUpdateEntityInstance(order_number);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  bridge().ServerEntityInstanceMetadataChanged(EntityInstanceMetadataChange(
      EntityInstanceMetadataChange::ADD, order_number.guid(),
      order_number.metadata()));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  bridge().ServerEntityInstanceMetadataChanged(EntityInstanceMetadataChange(
      EntityInstanceMetadataChange::UPDATE, order_number.guid(),
      order_number.metadata()));
}

// Tests that `ServerEntityInstanceMetadataChanged()` includes unknown fields
// from the server.
TEST_F(ValuableMetadataSyncBridgeTest,
       ServerEntityInstanceMetadataChanged_PreservesUnknownFields) {
  ON_CALL(mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
  const EntityInstance vehicle = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000"});
  entity_table().AddOrUpdateEntityInstance(vehicle);

  sync_pb::EntitySpecifics base_specifics;
  AddUnknownFieldToProto(*base_specifics.mutable_autofill_valuable_metadata(),
                         "unknown_field");
  EXPECT_CALL(mock_processor(),
              GetPossiblyTrimmedRemoteSpecifics(vehicle.guid().value()))
      .WillOnce(ReturnRef(base_specifics));

  EXPECT_CALL(mock_processor(), Put)
      .WillOnce([&vehicle](const std::string& storage_key,
                           std::unique_ptr<syncer::EntityData> entity_data,
                           syncer::MetadataChangeList* metadata) {
        ASSERT_EQ(storage_key, vehicle.guid().value());
        EXPECT_THAT(entity_data->specifics.autofill_valuable_metadata(),
                    HasUnknownField("unknown_field"));
      });

  bridge().ServerEntityInstanceMetadataChanged(EntityInstanceMetadataChange(
      EntityInstanceMetadataChange::ADD, vehicle.guid(), vehicle.metadata()));
}

// Tests that `ServerEntityInstanceMetadataChanged()` handles a REMOVE change.
TEST_F(ValuableMetadataSyncBridgeTest,
       ServerEntityInstanceMetadataChanged_Remove) {
  ON_CALL(mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
  const EntityInstance vehicle = CreateServerVehicleEntityInstance();

  EXPECT_CALL(mock_processor(), Delete(*vehicle.guid(), _, _));
  bridge().ServerEntityInstanceMetadataChanged(
      EntityInstanceMetadataChange(EntityInstanceMetadataChange::REMOVE,
                                   vehicle.guid(), vehicle.metadata()));
}

// Tests that DeleteOldOrphanMetadata() deletes metadata that has no
// corresponding data entity.
TEST_F(ValuableMetadataSyncBridgeTest, DeleteOldOrphanMetadata) {
  base::HistogramTester histogram_tester;

  // 1. Setup initial state with two server vehicles and three metadata entries,
  // one of which is an orphan.
  const EntityInstance server_vehicle1 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-2000-8000-300000000000"});
  const EntityInstance server_vehicle2 = CreateServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  entity_table().AddOrUpdateEntityInstance(server_vehicle1);
  entity_table().AddOrUpdateEntityInstance(server_vehicle2);

  const EntityInstance::EntityMetadata orphan_metadata =
      test::GetVehicleEntityInstance(
          {.guid = "00000000-0000-6000-8000-300000000000"})
          .metadata();

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *server_vehicle1.guid(),
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(
          server_vehicle1.metadata(),
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *server_vehicle2.guid(),
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(
          server_vehicle2.metadata(),
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      *orphan_metadata.guid,
      SpecificsToEntity(CreateSpecificsFromEntityMetadata(

          orphan_metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{}))));

  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(entity_change_list));

  ASSERT_THAT(
      GetEntityMetadataEntries(),
      UnorderedElementsAre(server_vehicle1.metadata(),
                           server_vehicle2.metadata(), orphan_metadata));

  // 2. Expect that the orphan metadata is deleted from the sync server.
  EXPECT_CALL(mock_processor(), Delete(*orphan_metadata.guid, _, _));
  EXPECT_CALL(backend(), CommitChanges());

  // 3. Restart the bridge to force the cleanup of orphan metadata.
  bridge_ = std::make_unique<ValuableMetadataSyncBridge>(
      mock_processor_.CreateForwardingProcessor(), &backend_);

  // 4. Verify that the orphan metadata is deleted locally and the UMA metric is
  // recorded.
  EXPECT_THAT(GetEntityMetadataEntries(),
              UnorderedElementsAre(server_vehicle1.metadata(),
                                   server_vehicle2.metadata()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ValuableMetadata.OrphanEntriesRemovedCount", 1, 1);
}

}  // namespace
}  // namespace autofill
