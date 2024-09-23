// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/collaboration_group_sync_bridge.h"

#include <memory>
#include <set>

#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {
namespace {

using base::test::EqualsProto;
using syncer::HasInitialSyncDone;
using syncer::IsEmptyMetadataBatch;
using syncer::NoModelError;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::IsNull;
using testing::SizeIs;
using testing::UnorderedElementsAre;

sync_pb::CollaborationGroupSpecifics MakeSpecifics(
    const GroupId& id,
    const base::Time& changed_at = base::Time::Now()) {
  sync_pb::CollaborationGroupSpecifics result;
  result.set_collaboration_id(id.value());
  result.set_changed_at_timestamp_millis_since_unix_epoch(
      changed_at.InMillisecondsSinceUnixEpoch());
  return result;
}

syncer::EntityData EntityDataFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_collaboration_group() = specifics;
  entity_data.name = specifics.collaboration_id();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> EntityChangeAddFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateAdd(specifics.collaboration_id(),
                                         EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeUpdateFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateUpdate(specifics.collaboration_id(),
                                            EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeDeleteFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateDelete(specifics.collaboration_id());
}

std::vector<sync_pb::CollaborationGroupSpecifics> ExtractSpecificsFromDataBatch(
    std::unique_ptr<syncer::DataBatch> data_batch) {
  std::vector<sync_pb::CollaborationGroupSpecifics> result;
  while (data_batch->HasNext()) {
    result.push_back(
        data_batch->Next().second->specifics.collaboration_group());
  }
  return result;
}

class MockObserver : public CollaborationGroupSyncBridge::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnGroupsUpdated,
              (const std::vector<GroupId>&,
               const std::vector<GroupId>&,
               const std::vector<GroupId>&),
              (override));
  MOCK_METHOD(void, OnDataLoaded, (), (override));
};

class CollaborationGroupSyncBridgeTest : public testing::Test {
 public:
  CollaborationGroupSyncBridgeTest()
      : data_type_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}
  ~CollaborationGroupSyncBridgeTest() override = default;

  void TearDown() override { bridge_->RemoveObserver(&observer_); }

  void CreateBridgeAndWaitForReadyToSync() {
    base::RunLoop run_loop;
    ON_CALL(mock_processor_, ModelReadyToSync)
        .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    bridge_ = std::make_unique<CollaborationGroupSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
            data_type_store_.get()));

    bridge_->AddObserver(&observer_);

    // Wait for ready to sync.
    run_loop.Run();
  }

  void RestartBridgeAndWaitForReadyToSync() {
    bridge_->RemoveObserver(&observer_);
    CreateBridgeAndWaitForReadyToSync();
  }

  CollaborationGroupSyncBridge& bridge() { return *bridge_; }

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() {
    return mock_processor_;
  }

  testing::NiceMock<MockObserver>& observer() { return observer_; }

  syncer::DataTypeStore& data_type_store() { return *data_type_store_; }

  std::vector<sync_pb::CollaborationGroupSpecifics> GetBridgeSpecifics() {
    return ExtractSpecificsFromDataBatch(bridge().GetAllDataForDebugging());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<syncer::DataTypeStore> data_type_store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  testing::NiceMock<MockObserver> observer_;
  std::unique_ptr<CollaborationGroupSyncBridge> bridge_;
};

TEST_F(CollaborationGroupSyncBridgeTest, ShouldReturnClientTag) {
  CreateBridgeAndWaitForReadyToSync();
  EXPECT_TRUE(bridge().SupportsGetClientTag());
  const GroupId id("collaboration1");
  EXPECT_THAT(bridge().GetClientTag(EntityDataFromSpecifics(MakeSpecifics(id))),
              Eq(id.value()));
}

TEST_F(CollaborationGroupSyncBridgeTest, ShouldReturnStorageKey) {
  CreateBridgeAndWaitForReadyToSync();
  EXPECT_TRUE(bridge().SupportsGetStorageKey());
  const GroupId id("collaboration1");
  EXPECT_THAT(
      bridge().GetStorageKey(EntityDataFromSpecifics(MakeSpecifics(id))),
      Eq(id.value()));
}

TEST_F(CollaborationGroupSyncBridgeTest, ShouldValidateEntityData) {
  CreateBridgeAndWaitForReadyToSync();
  EXPECT_TRUE(bridge().IsEntityDataValid(
      EntityDataFromSpecifics(MakeSpecifics(GroupId("id")))));
  // Specifics without `collaboration_id` considered invalid.
  EXPECT_FALSE(bridge().IsEntityDataValid(
      EntityDataFromSpecifics(sync_pb::CollaborationGroupSpecifics())));
}

TEST_F(CollaborationGroupSyncBridgeTest, ShouldMergeFullSyncData) {
  CreateBridgeAndWaitForReadyToSync();

  const GroupId id1("id1");
  const GroupId id2("id2");

  const sync_pb::CollaborationGroupSpecifics specifics1 =
      MakeSpecifics(id1, base::Time::FromMillisecondsSinceUnixEpoch(1000));
  const sync_pb::CollaborationGroupSpecifics specifics2 =
      MakeSpecifics(id2, base::Time::FromMillisecondsSinceUnixEpoch(2000));

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(EntityChangeAddFromSpecifics(specifics1));
  entity_changes.push_back(EntityChangeAddFromSpecifics(specifics2));

  // Mimics initial sync with two entities described above.
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(entity_changes));

  // Verify that bridge stores these two entities and nothing else.
  EXPECT_THAT(
      GetBridgeSpecifics(),
      UnorderedElementsAre(EqualsProto(specifics1), EqualsProto(specifics2)));
  EXPECT_THAT(bridge().GetCollaborationGroupIds(),
              UnorderedElementsAre(id1, id2));
}

TEST_F(CollaborationGroupSyncBridgeTest,
       ShouldApplyIncrementalSyncChangesAndNotifyObserver) {
  EXPECT_CALL(observer(), OnDataLoaded);
  CreateBridgeAndWaitForReadyToSync();
  testing::Mock::VerifyAndClearExpectations(&observer());

  const GroupId id1("id1");
  const GroupId id2("id2");

  const sync_pb::CollaborationGroupSpecifics specifics1 =
      MakeSpecifics(id1, base::Time::FromMillisecondsSinceUnixEpoch(1000));
  sync_pb::CollaborationGroupSpecifics specifics2 =
      MakeSpecifics(id2, base::Time::FromMillisecondsSinceUnixEpoch(2000));

  syncer::EntityChangeList intitial_entity_changes;
  intitial_entity_changes.push_back(EntityChangeAddFromSpecifics(specifics1));
  intitial_entity_changes.push_back(EntityChangeAddFromSpecifics(specifics2));

  // Mimics initial sync with two entities described above.
  EXPECT_CALL(observer(),
              OnGroupsUpdated(
                  /*added_group_ids*/ UnorderedElementsAre(id1, id2), _, _));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(intitial_entity_changes));
  testing::Mock::VerifyAndClearExpectations(&observer());

  // Verify that bridge stores these two entities and nothing else.
  EXPECT_THAT(
      GetBridgeSpecifics(),
      UnorderedElementsAre(EqualsProto(specifics1), EqualsProto(specifics2)));

  // Mimic incremental update: `specifics1` is deleted, `specifics2` is updated,
  // `specifics3` is added.
  const GroupId id3("id3");
  const sync_pb::CollaborationGroupSpecifics specifics3 =
      MakeSpecifics(id3, base::Time::FromMillisecondsSinceUnixEpoch(3000));
  specifics2.set_changed_at_timestamp_millis_since_unix_epoch(4000);

  syncer::EntityChangeList incremental_changes;
  incremental_changes.push_back(EntityChangeDeleteFromSpecifics(specifics1));
  incremental_changes.push_back(EntityChangeUpdateFromSpecifics(specifics2));
  incremental_changes.push_back(EntityChangeAddFromSpecifics(specifics3));

  EXPECT_CALL(observer(),
              OnGroupsUpdated(/*added_group_ids*/ ElementsAre(id3),
                              /*updated_group_ids*/ ElementsAre(id2),
                              /*deleted_group_ids*/ ElementsAre(id1)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(incremental_changes));

  // Verify that bridge stores`specifics3` and updated `specifics2`.
  EXPECT_THAT(
      GetBridgeSpecifics(),
      UnorderedElementsAre(EqualsProto(specifics2), EqualsProto(specifics3)));
  EXPECT_THAT(bridge().GetCollaborationGroupIds(),
              UnorderedElementsAre(id2, id3));
}

TEST_F(CollaborationGroupSyncBridgeTest, ShouldStoreAndLoadMetadata) {
  // The first call is expected in the very beginning when there is no metadata
  // yet. Use InSequence to guarantee the sequence of the expected calls.
  testing::InSequence sequence;
  EXPECT_CALL(mock_processor(), ModelReadyToSync(IsEmptyMetadataBatch()));

  CreateBridgeAndWaitForReadyToSync();

  // Simulate the initial sync merge.
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge().CreateMetadataChangeList();
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState::INITIAL_SYNC_DONE);
  metadata_changes->UpdateDataTypeState(data_type_state);
  bridge().MergeFullSyncData(std::move(metadata_changes),
                             syncer::EntityChangeList());

  // The following call should happen on the second initialization, and it
  // should have non-empty metadata.
  EXPECT_CALL(mock_processor(),
              ModelReadyToSync(MetadataBatchContains(HasInitialSyncDone(),
                                                     /*entities*/ IsEmpty())));
  // Simulate restarting the sync bridge.
  CreateBridgeAndWaitForReadyToSync();
}

TEST_F(CollaborationGroupSyncBridgeTest, ShouldStoreAndLoadData) {
  CreateBridgeAndWaitForReadyToSync();

  const GroupId id1("id1");
  const GroupId id2("id2");

  const sync_pb::CollaborationGroupSpecifics specifics1 =
      MakeSpecifics(id1, base::Time::FromMillisecondsSinceUnixEpoch(1000));
  sync_pb::CollaborationGroupSpecifics specifics2 =
      MakeSpecifics(id2, base::Time::FromMillisecondsSinceUnixEpoch(2000));

  syncer::EntityChangeList intitial_entity_changes;
  intitial_entity_changes.push_back(EntityChangeAddFromSpecifics(specifics1));
  intitial_entity_changes.push_back(EntityChangeAddFromSpecifics(specifics2));

  // Mimics initial sync with two entities described above.
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(intitial_entity_changes));

  // Simulate restarting the sync bridge.
  RestartBridgeAndWaitForReadyToSync();

  // Verify that bridge still stores these two entities.
  EXPECT_THAT(
      GetBridgeSpecifics(),
      UnorderedElementsAre(EqualsProto(specifics1), EqualsProto(specifics2)));
  EXPECT_THAT(bridge().GetCollaborationGroupIds(),
              UnorderedElementsAre(id1, id2));
}

TEST_F(CollaborationGroupSyncBridgeTest, ShouldApplyDisableSyncChanges) {
  CreateBridgeAndWaitForReadyToSync();

  // Mimics initial sync with some entities and metadata.
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge().CreateMetadataChangeList();
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState::INITIAL_SYNC_DONE);
  metadata_changes->UpdateDataTypeState(data_type_state);

  syncer::EntityChangeList intitial_entity_changes;
  intitial_entity_changes.push_back(
      EntityChangeAddFromSpecifics(MakeSpecifics(GroupId("id1"))));
  intitial_entity_changes.push_back(
      EntityChangeAddFromSpecifics(MakeSpecifics(GroupId("id2"))));

  // Mimics initial sync with some entities and metadata.
  bridge().MergeFullSyncData(std::move(metadata_changes),
                             std::move(intitial_entity_changes));

  // Should clear all data and metadata, `delete_metadata_change_list` is not
  // relevant for this implementation, so nullptr is okay.
  bridge().ApplyDisableSyncChanges(/*delete_metadata_change_list=*/nullptr);

  // Verify that bridge doesn't store specifics.
  EXPECT_THAT(GetBridgeSpecifics(), IsEmpty());

  // Verify that data and metadata was removed from disk as well.
  {
    base::RunLoop run_loop;
    base::MockOnceCallback<syncer::DataTypeStore::ReadAllDataCallback::RunType>
        get_all_data_callback;
    EXPECT_CALL(get_all_data_callback,
                Run(NoModelError(), /*data_records*/ Pointee(IsEmpty())))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    data_type_store().ReadAllData(get_all_data_callback.Get());
    run_loop.Run();
  }

  // Verify that metadata was removed from disk.
  {
    base::RunLoop run_loop;
    base::MockOnceCallback<syncer::DataTypeStore::ReadMetadataCallback::RunType>
        get_metadata_callback;
    EXPECT_CALL(get_metadata_callback,
                Run(NoModelError(), IsEmptyMetadataBatch()))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    data_type_store().ReadAllMetadata(get_metadata_callback.Get());
    run_loop.Run();
  }
}

TEST(CollaborationGroupSyncBridgeNoFixtureTest, ShouldReportIsDataLoaded) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<syncer::DataTypeStore> data_type_store =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor;
  testing::NiceMock<MockObserver> observer;

  CollaborationGroupSyncBridge bridge(
      mock_processor.CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
          data_type_store.get()));
  bridge.AddObserver(&observer);

  base::RunLoop run_loop;
  ON_CALL(mock_processor, ModelReadyToSync)
      .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Loading is asynchronous, so data loading shouldn't happen yet.
  EXPECT_FALSE(bridge.IsDataLoaded());

  // Data should be loaded by the time ModelReadyToSync() is called.
  EXPECT_CALL(observer, OnDataLoaded);
  run_loop.Run();
  EXPECT_TRUE(bridge.IsDataLoaded());
}

}  // namespace
}  // namespace data_sharing
