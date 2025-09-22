// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_sync_bridge.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/data_sharing/test_support/extended_shared_tab_group_account_data_specifics.pb.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/mock_data_type_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing::personal_collaboration_data {

namespace {

using base::test::EqualsProto;
using syncer::CollaborationId;
using testing::_;
using testing::Return;
using testing::ReturnRef;

const char kTestCollaborationId[] = "collaboration_id";
const char kTestTabGuid[] = "00000000-0000-0000-0000-000000000001";
const char kTestGroupGuid[] = "00000000-0000-0000-0000-000000000002";
const char kTestStorageKey[] =
    "00000000-0000-0000-0000-000000000001|collaboration_id";

class FakeWriteBatch : public syncer::DataTypeStore::WriteBatch {
 public:
  FakeWriteBatch()
      : metadata_change_list_(
            std::make_unique<syncer::InMemoryMetadataChangeList>()) {}
  ~FakeWriteBatch() override = default;

  void WriteData(const std::string& id, const std::string& value) override {}
  void DeleteData(const std::string& id) override {}
  syncer::MetadataChangeList* GetMetadataChangeList() override {
    return metadata_change_list_.get();
  }

 private:
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list_;
};

syncer::EntityData CreateEntityData(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics,
    base::Time creation_time = base::Time::Now()) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_account_data() = specifics;
  entity_data.name = specifics.guid();
  entity_data.creation_time = creation_time;
  return entity_data;
}

sync_pb::SharedTabGroupAccountDataSpecifics CreateTabGroupAccountSpecifics(
    const CollaborationId& collaboration_id,
    const std::string& tab_guid,
    const std::string& group_guid,
    const base::Time last_seen_timestamp) {
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.set_guid(tab_guid);
  specifics.set_collaboration_id(collaboration_id.value());
  sync_pb::SharedTabDetails* tab_details =
      specifics.mutable_shared_tab_details();
  tab_details->set_shared_tab_group_guid(group_guid);
  tab_details->set_last_seen_timestamp_windows_epoch(
      last_seen_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

sync_pb::SharedTabGroupAccountDataSpecifics
CreateTabGroupAccountSpecificsForGroup(const CollaborationId& collaboration_id,
                                       const std::string& group_guid,
                                       std::optional<size_t> position) {
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.set_guid(group_guid);
  specifics.set_collaboration_id(collaboration_id.value());
  sync_pb::SharedTabGroupDetails* shared_tab_group_details =
      specifics.mutable_shared_tab_group_details();
  if (position.has_value()) {
    shared_tab_group_details->set_pinned_position(position.value());
  }
  return specifics;
}

std::unique_ptr<syncer::EntityChange> CreateAddEntityChange(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics,
    base::Time creation_time = base::Time::Now()) {
  return syncer::EntityChange::CreateAdd(
      kTestStorageKey, CreateEntityData(specifics, creation_time));
}

class PersonalCollaborationDataSyncBridgeTest : public testing::Test {
 public:
  PersonalCollaborationDataSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void InitializeBridge() {
    ON_CALL(processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    ON_CALL(processor_, GetPossiblyTrimmedRemoteSpecifics(_))
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(processor_, ModelReadyToSync).WillOnce([&]() {
      quit_closure.Run();
    });

    bridge_ = std::make_unique<PersonalCollaborationDataSyncBridge>(
        processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));

    run_loop.Run();

    ASSERT_TRUE(bridge().IsInitialized());
  }

  void CreateBridgeWithMockStore(
      std::unique_ptr<syncer::MockDataTypeStore> store) {
    ON_CALL(processor_, IsTrackingMetadata()).WillByDefault(Return(true));

    syncer::OnceDataTypeStoreFactory factory = base::BindOnce(
        [](std::unique_ptr<syncer::DataTypeStore> store, syncer::DataType type,
           syncer::DataTypeStore::InitCallback callback) {
          std::move(callback).Run(/*error=*/std::nullopt, std::move(store));
        },
        std::move(store));

    bridge_ = std::make_unique<PersonalCollaborationDataSyncBridge>(
        processor_.CreateForwardingProcessor(), std::move(factory));
  }

  std::map<std::string, sync_pb::SharedTabGroupAccountDataSpecifics>
  GetTabDetailsInStore() {
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
    base::RunLoop run_loop;
    store_->ReadAllData(base::BindLambdaForTesting(
        [&run_loop, &entries](
            const std::optional<syncer::ModelError>& error,
            std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
          entries = std::move(data);
          run_loop.Quit();
        }));
    run_loop.Run();

    std::map<std::string, sync_pb::SharedTabGroupAccountDataSpecifics> result;
    for (const auto& record : *entries) {
      sync_pb::SharedTabGroupAccountDataSpecifics specifics;
      if (!specifics.ParseFromString(record.value)) {
        CHECK(false);
      }

      if (specifics.has_shared_tab_details()) {
        result[record.id] = specifics;
      }
    }

    return result;
  }

  size_t GetNumTabDetailsInStore() { return GetTabDetailsInStore().size(); }

  size_t GetDataBatchCount(std::unique_ptr<syncer::DataBatch> batch) {
    size_t count = 0;
    while (batch && batch->HasNext()) {
      batch->Next();
      count++;
    }
    return count;
  }

  PersonalCollaborationDataSyncBridge& bridge() { return *bridge_; }
  syncer::DataTypeStore& store() { return *store_; }
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() {
    return processor_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  std::unique_ptr<PersonalCollaborationDataSyncBridge> bridge_;
};

TEST_F(PersonalCollaborationDataSyncBridgeTest, ShouldCheckValidEntities) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  base::Time last_seen_time = base::Time::Now();
  // Valid tab entity.
  EXPECT_TRUE(bridge().IsEntityDataValid(CreateEntityData(
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, last_seen_time),
      base::Time::Now())));

  // Valid group entity.
  EXPECT_TRUE(bridge().IsEntityDataValid(
      CreateEntityData(CreateTabGroupAccountSpecificsForGroup(
          kCollaborationId, kTestGroupGuid, 0))));

  // Invalid: invalid tab guid.
  sync_pb::SharedTabGroupAccountDataSpecifics invalid_tab_guid_specifics =
      CreateTabGroupAccountSpecifics(kCollaborationId, "invalid-guid",
                                     kTestGroupGuid, last_seen_time);
  EXPECT_FALSE(
      bridge().IsEntityDataValid(CreateEntityData(invalid_tab_guid_specifics)));

  // Invalid: empty collaboration ID.
  sync_pb::SharedTabGroupAccountDataSpecifics empty_collab_id_specifics =
      CreateTabGroupAccountSpecifics(CollaborationId(), kTestTabGuid,
                                     kTestGroupGuid, last_seen_time);
  empty_collab_id_specifics.set_collaboration_id("");
  EXPECT_FALSE(
      bridge().IsEntityDataValid(CreateEntityData(empty_collab_id_specifics)));

  // Invalid: invalid group guid in tab details.
  sync_pb::SharedTabGroupAccountDataSpecifics invalid_group_guid_specifics =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     "invalid-guid", last_seen_time);
  EXPECT_FALSE(bridge().IsEntityDataValid(
      CreateEntityData(invalid_group_guid_specifics)));

  // Invalid: missing timestamp in tab details.
  sync_pb::SharedTabGroupAccountDataSpecifics missing_ts_specifics =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time());
  missing_ts_specifics.mutable_shared_tab_details()
      ->clear_last_seen_timestamp_windows_epoch();
  EXPECT_FALSE(
      bridge().IsEntityDataValid(CreateEntityData(missing_ts_specifics)));

  // Invalid: no details.
  sync_pb::SharedTabGroupAccountDataSpecifics no_details_specifics;
  no_details_specifics.set_guid(kTestTabGuid);
  no_details_specifics.set_collaboration_id(kTestCollaborationId);
  EXPECT_FALSE(
      bridge().IsEntityDataValid(CreateEntityData(no_details_specifics)));
}

TEST_F(PersonalCollaborationDataSyncBridgeTest, ShouldResolveConflicts) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  // Test kUseLocal: local data is newer.
  sync_pb::SharedTabGroupAccountDataSpecifics local_newer =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time::Now());
  sync_pb::SharedTabGroupAccountDataSpecifics remote_older =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid,
                                     base::Time::Now() - base::Seconds(5));

  syncer::EntityChangeList change_list1;
  change_list1.push_back(CreateAddEntityChange(local_newer));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list1));

  EXPECT_EQ(syncer::ConflictResolution::kUseLocal,
            bridge().ResolveConflict(kTestStorageKey,
                                     CreateEntityData(remote_older)));

  // Test kUseRemote: remote data is newer.
  sync_pb::SharedTabGroupAccountDataSpecifics local_older =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time::Now());
  sync_pb::SharedTabGroupAccountDataSpecifics remote_newer =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid,
                                     base::Time::Now() + base::Seconds(42));

  syncer::EntityChangeList change_list2;
  change_list2.push_back(CreateAddEntityChange(local_older));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list2));

  EXPECT_EQ(syncer::ConflictResolution::kUseRemote,
            bridge().ResolveConflict(kTestStorageKey,
                                     CreateEntityData(remote_newer)));

  // Test kChangesMatch: timestamps are the same.
  base::Time now = base::Time::Now();
  sync_pb::SharedTabGroupAccountDataSpecifics local_same_time =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, now);
  sync_pb::SharedTabGroupAccountDataSpecifics remote_same_time =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, now);

  syncer::EntityChangeList change_list3;
  change_list3.push_back(CreateAddEntityChange(local_same_time));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list3));

  EXPECT_EQ(syncer::ConflictResolution::kChangesMatch,
            bridge().ResolveConflict(kTestStorageKey,
                                     CreateEntityData(remote_same_time)));
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldResolveConflictsWithUntrackedOrMismatchedData) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  // Remote data for a key that is not tracked locally should be used.
  sync_pb::SharedTabGroupAccountDataSpecifics untracked_remote =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time::Now());
  EXPECT_EQ(syncer::ConflictResolution::kUseRemote,
            bridge().ResolveConflict(kTestStorageKey,
                                     CreateEntityData(untracked_remote)));

  // Test conflict between tab details and group details. Remote should be used.
  sync_pb::SharedTabGroupAccountDataSpecifics local_tab_details =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time::Now());
  sync_pb::SharedTabGroupAccountDataSpecifics remote_group_details =
      CreateTabGroupAccountSpecificsForGroup(kCollaborationId, kTestTabGuid, 0);

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(local_tab_details));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));

  EXPECT_EQ(syncer::ConflictResolution::kUseRemote,
            bridge().ResolveConflict(kTestStorageKey,
                                     CreateEntityData(remote_group_details)));
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldClearStoreOnApplyDisable) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  base::Time last_seen_time = base::Time::Now();
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, kTestTabGuid, kTestGroupGuid, last_seen_time)));

  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));
  EXPECT_EQ(GetNumTabDetailsInStore(), 1u);

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldTrimAllSupportedFieldsFromSharedTabDetailsSpecifics) {
  InitializeBridge();

  sync_pb::EntitySpecifics remote_account_data_specifics;
  sync_pb::SharedTabGroupAccountDataSpecifics* account_data_specifics =
      remote_account_data_specifics.mutable_shared_tab_group_account_data();
  account_data_specifics->set_guid("guid");
  account_data_specifics->set_collaboration_id("collaboration_id");
  account_data_specifics->set_update_time_windows_epoch_micros(1234567890);
  account_data_specifics->mutable_shared_tab_details()
      ->set_shared_tab_group_guid("shared_tab_group_guid");
  account_data_specifics->mutable_shared_tab_details()
      ->set_last_seen_timestamp_windows_epoch(3214567890);

  EXPECT_THAT(bridge().TrimAllSupportedFieldsFromRemoteSpecifics(
                  remote_account_data_specifics),
              EqualsProto(sync_pb::EntitySpecifics()));

  sync_pb::EntitySpecifics remote_account_data_specifics2;
  sync_pb::SharedTabGroupAccountDataSpecifics* account_data_specifics2 =
      remote_account_data_specifics2.mutable_shared_tab_group_account_data();
  account_data_specifics2->set_guid("guid");
  account_data_specifics2->set_collaboration_id("collaboration_id");
  account_data_specifics2->set_update_time_windows_epoch_micros(1234567890);
  account_data_specifics2->mutable_shared_tab_group_details()
      ->set_pinned_position(11);
  account_data_specifics2->set_version(999);

  EXPECT_THAT(bridge().TrimAllSupportedFieldsFromRemoteSpecifics(
                  remote_account_data_specifics2),
              EqualsProto(sync_pb::EntitySpecifics()));
}

TEST_F(
    PersonalCollaborationDataSyncBridgeTest,
    ShouldKeepUnknownFieldsFromSharedTabAccountDataSpecifics_SharedTabDetails) {
  InitializeBridge();

  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      extended_account_data_specifics;
  extended_account_data_specifics.set_guid("guid");
  extended_account_data_specifics.set_collaboration_id("collaboration_id");
  extended_account_data_specifics.mutable_shared_tab_details()
      ->set_extra_field_for_testing("extra_field_for_testing");

  // Serialize and deserialize the proto to get unknown fields.
  sync_pb::EntitySpecifics remote_entity_specifics;
  ASSERT_TRUE(remote_entity_specifics.mutable_shared_tab_group_account_data()
                  ->ParseFromString(
                      extended_account_data_specifics.SerializeAsString()));

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(
          remote_entity_specifics);
  EXPECT_FALSE(trimmed_specifics.shared_tab_group_account_data().has_guid());
  EXPECT_FALSE(
      trimmed_specifics.shared_tab_group_account_data().has_collaboration_id());

  // Verify that deserialized proto keeps unknown fields.
  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      deserialized_extended_specifics;
  ASSERT_TRUE(deserialized_extended_specifics.ParseFromString(
      trimmed_specifics.shared_tab_group_account_data().SerializeAsString()));
  EXPECT_EQ(deserialized_extended_specifics.shared_tab_details()
                .extra_field_for_testing(),
            "extra_field_for_testing");
}

TEST_F(
    PersonalCollaborationDataSyncBridgeTest,
    ShouldKeepUnknownFieldsFromSharedTabAccountDataSpecifics_SharedTabGroupDetails) {
  InitializeBridge();

  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      extended_account_data_specifics;
  extended_account_data_specifics.set_guid("guid");
  extended_account_data_specifics.set_collaboration_id("collaboration_id");
  extended_account_data_specifics.mutable_shared_tab_group_details()
      ->set_extra_field_for_testing("extra_field_for_testing");

  // Serialize and deserialize the proto to get unknown fields.
  sync_pb::EntitySpecifics remote_entity_specifics;
  ASSERT_TRUE(remote_entity_specifics.mutable_shared_tab_group_account_data()
                  ->ParseFromString(
                      extended_account_data_specifics.SerializeAsString()));

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(
          remote_entity_specifics);
  EXPECT_FALSE(trimmed_specifics.shared_tab_group_account_data().has_guid());
  EXPECT_FALSE(
      trimmed_specifics.shared_tab_group_account_data().has_collaboration_id());

  // Verify that deserialized proto keeps unknown fields.
  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      deserialized_extended_specifics;
  ASSERT_TRUE(deserialized_extended_specifics.ParseFromString(
      trimmed_specifics.shared_tab_group_account_data().SerializeAsString()));
  EXPECT_EQ(deserialized_extended_specifics.shared_tab_group_details()
                .extra_field_for_testing(),
            "extra_field_for_testing");
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldKeepUnknownFieldsFromSharedTabAccountDataSpecifics_TopLevel) {
  InitializeBridge();

  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      extended_account_data_specifics;
  extended_account_data_specifics.set_guid("guid");
  extended_account_data_specifics.set_collaboration_id("collaboration_id");
  extended_account_data_specifics.set_extra_field_for_testing(
      "extra_field_for_testing");

  // Serialize and deserialize the proto to get unknown fields.
  sync_pb::EntitySpecifics remote_entity_specifics;
  ASSERT_TRUE(remote_entity_specifics.mutable_shared_tab_group_account_data()
                  ->ParseFromString(
                      extended_account_data_specifics.SerializeAsString()));

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(
          remote_entity_specifics);
  EXPECT_FALSE(trimmed_specifics.shared_tab_group_account_data().has_guid());
  EXPECT_FALSE(
      trimmed_specifics.shared_tab_group_account_data().has_collaboration_id());

  // Verify that deserialized proto keeps unknown fields.
  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      deserialized_extended_specifics;
  ASSERT_TRUE(deserialized_extended_specifics.ParseFromString(
      trimmed_specifics.shared_tab_group_account_data().SerializeAsString()));
  EXPECT_EQ(deserialized_extended_specifics.extra_field_for_testing(),
            "extra_field_for_testing");
}

TEST_F(PersonalCollaborationDataSyncBridgeTest, ShouldMergeFullSyncData) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      CreateAddEntityChange(CreateTabGroupAccountSpecifics(
          kCollaborationId, kTestTabGuid, kTestGroupGuid, base::Time::Now())));

  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(entity_change_list));

  EXPECT_EQ(GetNumTabDetailsInStore(), 1u);
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldGetSpecificsForStorageKey) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  // Test with a key that doesn't exist.
  EXPECT_FALSE(
      bridge().GetSpecificsForStorageKey("non-existent-key").has_value());

  // Test with a key that exists.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time::Now());
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(specifics));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));

  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> result =
      bridge().GetSpecificsForStorageKey(kTestStorageKey);
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(*result, EqualsProto(specifics));
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldReturnNulloptForNonexistentStorageKey) {
  InitializeBridge();
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey("nonexistent_key"));
}

TEST_F(PersonalCollaborationDataSyncBridgeTest, ShouldCreateAndGetSpecifics) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  // Create and add specifics.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time::Now());
  bridge().CreateOrUpdateSpecifics(kTestStorageKey, specifics);

  // Get the specifics and verify.
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> result =
      bridge().GetSpecificsForStorageKey(kTestStorageKey);
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(*result, EqualsProto(specifics));

  // Update the specifics.
  sync_pb::SharedTabGroupAccountDataSpecifics updated_specifics =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid,
                                     base::Time::Now() + base::Seconds(10));
  bridge().CreateOrUpdateSpecifics(kTestStorageKey, updated_specifics);

  // Get the updated specifics and verify.
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> updated_result =
      bridge().GetSpecificsForStorageKey(kTestStorageKey);
  EXPECT_TRUE(updated_result.has_value());
  EXPECT_THAT(*updated_result, EqualsProto(updated_specifics));
}

TEST_F(PersonalCollaborationDataSyncBridgeTest, ShouldRemoveSpecifics) {
  const CollaborationId kCollaborationId(kTestCollaborationId);
  InitializeBridge();

  // Create and add specifics.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics =
      CreateTabGroupAccountSpecifics(kCollaborationId, kTestTabGuid,
                                     kTestGroupGuid, base::Time::Now());
  bridge().CreateOrUpdateSpecifics(kTestStorageKey, specifics);

  // Verify the specifics exist.
  EXPECT_TRUE(bridge().GetSpecificsForStorageKey(kTestStorageKey).has_value());
  EXPECT_EQ(GetNumTabDetailsInStore(), 1u);

  // Remove the specifics.
  bridge().RemoveSpecifics(kTestStorageKey);

  // Verify the specifics are removed.
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey(kTestStorageKey).has_value());
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);
}

TEST_F(PersonalCollaborationDataSyncBridgeTest, ShouldHandleNoData) {
  InitializeBridge();

  // Should be empty initially.
  EXPECT_EQ(GetDataBatchCount(bridge().GetAllDataForDebugging()), 0u);
  EXPECT_EQ(GetDataBatchCount(bridge().GetDataForCommit({})), 0u);
  EXPECT_EQ(GetDataBatchCount(bridge().GetDataForCommit({"some_key"})), 0u);
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldReportErrorOnLoadFailure) {
  auto mock_store = std::make_unique<syncer::MockDataTypeStore>();
  EXPECT_CALL(*mock_store, ReadAllDataAndMetadata)
      .WillOnce([](syncer::DataTypeStore::ReadAllDataAndMetadataCallback
                       callback) {
        std::move(callback).Run(
            syncer::ModelError(
                FROM_HERE,
                syncer::ModelError::Type::kDataTypeStoreBackendDbReadFailed),
            /*entries=*/nullptr, /*metadata_batch=*/nullptr);
      });

  base::RunLoop run_loop;
  EXPECT_CALL(mock_processor(), ReportError(_))
      .WillOnce([&](const syncer::ModelError& error) { run_loop.Quit(); });
  CreateBridgeWithMockStore(std::move(mock_store));
  run_loop.Run();
  EXPECT_FALSE(bridge().IsInitialized());
}

TEST_F(PersonalCollaborationDataSyncBridgeTest,
       ShouldReportErrorOnCommitFailure) {
  auto mock_store = std::make_unique<syncer::MockDataTypeStore>();

  // Mock ReadAllDataAndMetadata to succeed.
  EXPECT_CALL(*mock_store, ReadAllDataAndMetadata)
      .WillOnce(
          [](syncer::DataTypeStore::ReadAllDataAndMetadataCallback callback) {
            std::move(callback).Run(
                /*error=*/std::nullopt,
                std::make_unique<syncer::DataTypeStore::RecordList>(),
                std::make_unique<syncer::MetadataBatch>());
          });

  // Mock CreateWriteBatch to return a valid batch.
  ON_CALL(*mock_store, CreateWriteBatch).WillByDefault([]() {
    return std::make_unique<FakeWriteBatch>();
  });

  // Mock CommitWriteBatch to fail.
  EXPECT_CALL(*mock_store, CommitWriteBatch)
      .WillOnce(
          [](std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch,
             syncer::DataTypeStore::CallbackWithResult callback) {
            std::move(callback).Run(syncer::ModelError(
                FROM_HERE,
                syncer::ModelError::Type::kDataTypeStoreBackendDbWriteFailed));
          });

  base::RunLoop run_loop;
  EXPECT_CALL(mock_processor(), ModelReadyToSync).WillOnce([&]() {
    run_loop.Quit();
  });
  CreateBridgeWithMockStore(std::move(mock_store));
  run_loop.Run();
  ASSERT_TRUE(bridge().IsInitialized());

  base::RunLoop commit_run_loop;
  EXPECT_CALL(mock_processor(), ReportError(_))
      .WillOnce(
          [&](const syncer::ModelError& error) { commit_run_loop.Quit(); });
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      CollaborationId(kTestCollaborationId), kTestTabGuid, kTestGroupGuid,
      base::Time::Now())));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));
  commit_run_loop.Run();
}

}  // namespace
}  // namespace data_sharing::personal_collaboration_data
