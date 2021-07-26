// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_sync_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "ash/public/cpp/desk_template.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/sync/engine/entity_data.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "components/sync/test/model/model_type_store_test_util.h"
#include "components/sync/test/model/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

using ash::DeskTemplate;
using sync_pb::ModelTypeState;
using sync_pb::WorkspaceDeskSpecifics;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::HasEncryptionKeyName;
using syncer::InMemoryMetadataChangeList;
using syncer::MetadataBatchContains;
using syncer::MetadataChangeList;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelError;
using syncer::ModelTypeStore;
using syncer::ModelTypeStoreTestUtil;
using testing::_;
using testing::Return;
using testing::SizeIs;
using testing::StrEq;

constexpr char kUuidFormat[] = "9e186d5a-502e-49ce-9ee1-00000000000%d";
constexpr char kNameFormat[] = "template %d";
const base::GUID kTestUuid1 =
    base::GUID::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 1));
const base::GUID kTestUuid2 =
    base::GUID::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 2));

WorkspaceDeskSpecifics CreateWorkspaceDeskSpecifics(
    int templateIndex,
    base::Time created_time = base::Time::Now()) {
  WorkspaceDeskSpecifics specifics;
  specifics.set_uuid(base::StringPrintf(kUuidFormat, templateIndex));
  specifics.set_name(base::StringPrintf(kNameFormat, templateIndex));
  specifics.set_created_time_usec(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

ModelTypeState StateWithEncryption(const std::string& encryption_key_name) {
  ModelTypeState state;
  state.set_encryption_key_name(encryption_key_name);
  return state;
}

class MockDeskModelObserver : public DeskModelObserver {
 public:
  MOCK_METHOD1(EntriesAddedOrUpdatedRemotely,
               void(const std::vector<const DeskTemplate*>&));
  MOCK_METHOD1(EntriesRemovedRemotely, void(const std::vector<std::string>&));
  MOCK_METHOD1(EntriesAddedOrUpdatedLocally,
               void(const std::vector<const DeskTemplate*>&));
  MOCK_METHOD1(EntriesRemovedLocally, void(const std::vector<std::string>&));
};

MATCHER_P(UuidIs, e, "") {
  return testing::ExplainMatchResult(e, arg->uuid(), result_listener);
}

class DeskSyncBridgeTest : public testing::Test {
 public:
  DeskSyncBridgeTest(const DeskSyncBridgeTest&) = delete;
  DeskSyncBridgeTest& operator=(const DeskSyncBridgeTest&) = delete;

 protected:
  static void VerifyAddOrUpdateEntrySuccess(
      DeskModel::AddOrUpdateEntryStatus status) {
    EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
  }

  static void VerifyAddOrUpdateEntryFailure(
      DeskModel::AddOrUpdateEntryStatus status) {
    EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kFailure);
  }

  static void VerifyDeleteEntrySuccess(DeskModel::DeleteEntryStatus status) {
    EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
  }

  DeskSyncBridgeTest()
      : store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void CreateBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    bridge_ = std::make_unique<DeskSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
    bridge_->AddObserver(&mock_observer_);
  }

  void FinishInitialization() { base::RunLoop().RunUntilIdle(); }

  void InitializeBridge() {
    CreateBridge();
    FinishInitialization();
  }

  void DisableBridgeSync() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(false));
  }

  void ShutdownBridge() {
    base::RunLoop().RunUntilIdle();
    bridge_->RemoveObserver(&mock_observer_);
  }

  void RestartBridge() {
    ShutdownBridge();
    InitializeBridge();
  }

  void WriteToStoreWithMetadata(
      const std::vector<WorkspaceDeskSpecifics>& specifics_list,
      ModelTypeState state) {
    std::unique_ptr<ModelTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.uuid(), specifics.SerializeAsString());
    }
    batch->GetMetadataChangeList()->UpdateModelTypeState(state);
    CommitToStoreAndWait(std::move(batch));
  }

  void CommitToStoreAndWait(std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
    base::RunLoop loop;
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            [](base::RunLoop* loop, const absl::optional<ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              loop->Quit();
            },
            &loop));
    loop.Run();
  }

  EntityData MakeEntityData(
      const WorkspaceDeskSpecifics& workspace_desk_specifics) {
    EntityData entity_data;

    *entity_data.specifics.mutable_workspace_desk() = workspace_desk_specifics;
    entity_data.name = workspace_desk_specifics.name();
    return entity_data;
  }

  EntityData MakeEntityData(const DeskTemplate& desk_template) {
    return MakeEntityData(DeskSyncBridge::AsSyncProto(&desk_template));
  }

  // Helper method to reduce duplicated code between tests. Wraps the given
  // specifics objects in an EntityData and EntityChange of type ACTION_ADD, and
  // returns an EntityChangeList containing them all. Order is maintained.
  EntityChangeList EntityAddList(
      const std::vector<WorkspaceDeskSpecifics>& specifics_list) {
    EntityChangeList changes;
    for (const auto& specifics : specifics_list) {
      changes.push_back(
          EntityChange::CreateAdd(specifics.uuid(), MakeEntityData(specifics)));
    }
    return changes;
  }

  base::Time AdvanceAndGetTime(
      base::TimeDelta delta = base::TimeDelta::FromMilliseconds(10)) {
    clock_.Advance(delta);
    return clock_.Now();
  }

  void AddTwoTemplates() {
    bridge_->AddOrUpdateEntry(
        std::make_unique<DeskTemplate>(kTestUuid1.AsLowercaseString(),
                                       "template 1", AdvanceAndGetTime()),
        base::BindOnce(DeskSyncBridgeTest::VerifyAddOrUpdateEntrySuccess));
    bridge_->AddOrUpdateEntry(
        std::make_unique<DeskTemplate>(kTestUuid2.AsLowercaseString(),
                                       "template 2", AdvanceAndGetTime()),
        base::BindOnce(DeskSyncBridgeTest::VerifyAddOrUpdateEntrySuccess));
  }

  MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  DeskSyncBridge* bridge() { return bridge_.get(); }

  MockDeskModelObserver* mock_observer() { return &mock_observer_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::SimpleTestClock clock_;

  // In memory model type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<ModelTypeStore> store_;

  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;

  std::unique_ptr<DeskSyncBridge> bridge_;

  testing::NiceMock<MockDeskModelObserver> mock_observer_;
};

TEST_F(DeskSyncBridgeTest, IsBridgeReady) {
  CreateBridge();
  ASSERT_FALSE(bridge()->IsReady());

  FinishInitialization();
  ASSERT_TRUE(bridge()->IsReady());
}

TEST_F(DeskSyncBridgeTest, IsBridgeSyncing) {
  InitializeBridge();
  ASSERT_TRUE(bridge()->IsSyncing());

  DisableBridgeSync();
  ASSERT_FALSE(bridge()->IsSyncing());
}

TEST_F(DeskSyncBridgeTest, InitializationWithLocalDataAndMetadata) {
  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  ModelTypeState state = StateWithEncryption("test_encryption_key");
  WriteToStoreWithMetadata({template1, template2}, state);
  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("test_encryption_key"),
                                /*entities=*/_)));

  InitializeBridge();

  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());

  // Verify both local specifics are loaded correctly.
  EXPECT_EQ(DeskSyncBridge::AsSyncProto(
                bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template1.uuid())))
                .SerializeAsString(),
            template1.SerializeAsString());

  EXPECT_EQ(DeskSyncBridge::AsSyncProto(
                bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template2.uuid())))
                .SerializeAsString(),
            template2.SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, AddEntriesLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllUuids().size());

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());
}

TEST_F(DeskSyncBridgeTest, AddEntryShouldSucceedWheSyncIsDisabled) {
  InitializeBridge();
  DisableBridgeSync();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  // Add entry should fail when the sync bridge is not ready.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(1);
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(kTestUuid1.AsLowercaseString(),
                                     "template 1", AdvanceAndGetTime()),
      base::BindOnce(DeskSyncBridgeTest::VerifyAddOrUpdateEntrySuccess));
}

TEST_F(DeskSyncBridgeTest, AddEntryShouldFailWhenBridgeIsNotReady) {
  // Only create sync bridge but do not allow it to finish initialization.
  CreateBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  // Add entry should fail when the sync bridge is not ready.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(kTestUuid1.AsLowercaseString(),
                                     "template 1", AdvanceAndGetTime()),
      base::BindOnce(DeskSyncBridgeTest::VerifyAddOrUpdateEntryFailure));
}

TEST_F(DeskSyncBridgeTest, UpdateEntryLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());

  // Update template 1
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(1);
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(kTestUuid1.AsLowercaseString(),
                                     "updated template 1", AdvanceAndGetTime()),
      base::BindOnce(DeskSyncBridgeTest::VerifyAddOrUpdateEntrySuccess));

  // We should still have both templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());
  // Template 1 should be updated.
  EXPECT_EQ(
      base::UTF16ToUTF8(bridge()->GetEntryByUUID(kTestUuid1)->template_name()),
      "updated template 1");

  // Template 2 should be unchanged.
  EXPECT_EQ(
      base::UTF16ToUTF8(bridge()->GetEntryByUUID(kTestUuid2)->template_name()),
      "template 2");
}

TEST_F(DeskSyncBridgeTest, DeleteEntryLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());

  // Delete template 1.
  bridge()->DeleteEntry(
      kTestUuid1.AsLowercaseString(),
      base::BindOnce(DeskSyncBridgeTest::VerifyDeleteEntrySuccess));

  // We should have only 1 template.
  EXPECT_EQ(1ul, bridge()->GetAllUuids().size());
  // Template 2 should be unchanged.
  EXPECT_EQ(
      base::UTF16ToUTF8(bridge()->GetEntryByUUID(kTestUuid2)->template_name()),
      "template 2");
}

TEST_F(DeskSyncBridgeTest, DeleteAllEntriesLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());

  // Delete all templates.
  bridge()->DeleteAllEntries(
      base::BindOnce(DeskSyncBridgeTest::VerifyDeleteEntrySuccess));

  // We should have no templates.
  EXPECT_EQ(0ul, bridge()->GetAllUuids().size());
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesEmpty) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesWithTwoAdditions) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  auto error =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 EntityAddList({template1, template2}));
  EXPECT_FALSE(error);

  // We should have two templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesWithOneUpdate) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({template1, template2}));

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());

  // Now update template 1 with a new content.
  WorkspaceDeskSpecifics updated_template1 = CreateWorkspaceDeskSpecifics(1);
  updated_template1.set_name("updated template 1");

  EntityChangeList update_changes;
  update_changes.push_back(EntityChange::CreateUpdate(
      template1.uuid(), MakeEntityData(updated_template1)));

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(1)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(update_changes));
  // We should still have both templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());
  // Template 1 should be updated to new content.
  EXPECT_EQ(DeskSyncBridge::AsSyncProto(
                bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template1.uuid())))
                .SerializeAsString(),
            updated_template1.SerializeAsString());
  EXPECT_EQ(DeskSyncBridge::AsSyncProto(
                bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template2.uuid())))
                .SerializeAsString(),
            template2.SerializeAsString());
}

// Tests that remote desk template can be correctly removed.
TEST_F(DeskSyncBridgeTest, ApplySyncChangesWithOneDeletion) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({template1, template2}));

  // Verify that we have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());

  // Now delete template 1.
  EntityChangeList delete_changes;
  delete_changes.push_back(EntityChange::CreateDelete(template1.uuid()));

  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(SizeIs(1)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(delete_changes));

  // Verify that we only have template 2.
  EXPECT_EQ(1ul, bridge()->GetAllUuids().size());
  EXPECT_EQ(DeskSyncBridge::AsSyncProto(
                bridge()->GetEntryByUUID(
                    base::GUID::ParseCaseInsensitive(template2.uuid())))
                .SerializeAsString(),
            template2.SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, ApplySyncChangesDeleteNonexistent) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();

  EXPECT_CALL(*processor(), Delete(_, _)).Times(0);

  EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete("no-such-uuid"));
  auto error = bridge()->ApplySyncChanges(std::move(metadata_changes),
                                          std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(DeskSyncBridgeTest, MergeSyncDataWithTwoEntries) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  auto metadata_change_list = std::make_unique<InMemoryMetadataChangeList>();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->MergeSyncData(std::move(metadata_change_list),
                          EntityAddList({template1, template2}));
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());
}

TEST_F(DeskSyncBridgeTest, MergeSyncDataUploadsLocalOnlyEntries) {
  InitializeBridge();

  // Seed two templates.
  // Seeded templates will be "template 1" and "template 2".
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllUuids().size());

  // Create server-side templates "template 2" and "template 3".
  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(2);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(3);

  auto metadata_change_list = std::make_unique<InMemoryMetadataChangeList>();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));

  // MergeSyncData should upload the local-only template "template 1".
  EXPECT_CALL(*processor(), Put(StrEq(kTestUuid1.AsLowercaseString()), _, _))
      .Times(1);

  bridge()->MergeSyncData(std::move(metadata_change_list),
                          EntityAddList({template1, template2}));

  // Merged data should contain 3 templtes.
  EXPECT_EQ(3ul, bridge()->GetAllUuids().size());
}

}  // namespace

}  // namespace desks_storage