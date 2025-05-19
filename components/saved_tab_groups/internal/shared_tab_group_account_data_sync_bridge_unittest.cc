// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_account_data_sync_bridge.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/test_support/extended_shared_tab_group_account_data_specifics.pb.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

namespace {

using base::test::EqualsProto;
using testing::_;
using testing::DefaultValue;
using testing::Eq;
using testing::Invoke;
using testing::Matcher;
using testing::Not;
using testing::Return;
using testing::SaveArgPointee;
using testing::Sequence;
using testing::UnorderedElementsAre;

// Action SaveArgPointeeMove<k>(pointer) saves the value pointed to by the k-th
// (0-based) argument of the mock function by moving it to *pointer.
ACTION_TEMPLATE(SaveArgPointeeMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(*testing::get<k>(args));
}

MATCHER_P3(HasAccountMetadata, title, color, collaboration_id, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color &&
         arg.collaboration_id() == CollaborationId(collaboration_id);
}

// Create the client-tag/storage-key for an entity.
std::string GetClientTagFromSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  return specifics.guid() + "|" + specifics.collaboration_id();
}

// Create the client-tag/storage-key for a saved tab.
std::string CreateClientTagForSharedTab(const SavedTabGroup& group,
                                        const SavedTabGroupTab& tab) {
  return tab.saved_tab_guid().AsLowercaseString() + "|" +
         group.collaboration_id().value().value();
}

// Create the client-tag/storage-key for a saved tab.
std::string CreateClientTagForSharedTab(const CollaborationId& collaboration_id,
                                        const base::Uuid& tab_guid) {
  return tab_guid.AsLowercaseString() + "|" + collaboration_id.value();
}

sync_pb::SharedTabGroupAccountDataSpecifics CreateTabGroupAccountSpecifics(
    const CollaborationId& collaboration_id,
    const SavedTabGroupTab& tab,
    const base::Time last_seen_timestamp) {
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
  specifics.set_collaboration_id(collaboration_id.value());
  sync_pb::SharedTabDetails* tab_group_details =
      specifics.mutable_shared_tab_details();
  tab_group_details->set_shared_tab_group_guid(
      tab.saved_group_guid().AsLowercaseString());
  tab_group_details->set_last_seen_timestamp_windows_epoch(
      last_seen_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

syncer::EntityData CreateEntityData(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics,
    base::Time creation_time = base::Time::Now()) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_account_data() = specifics;
  entity_data.name = specifics.guid();
  entity_data.creation_time = creation_time;
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateAddEntityChange(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics,
    base::Time creation_time = base::Time::Now()) {
  const std::string& storage_key = GetClientTagFromSpecifics(specifics);
  return syncer::EntityChange::CreateAdd(
      storage_key, CreateEntityData(specifics, creation_time));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateEntityChange(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics,
    base::Time creation_time = base::Time::Now()) {
  const std::string& storage_key = GetClientTagFromSpecifics(specifics);
  return syncer::EntityChange::CreateUpdate(
      storage_key, CreateEntityData(specifics, creation_time));
}

std::unique_ptr<syncer::EntityChange> CreateDeleteEntityChange(
    const std::string& storage_key) {
  return syncer::EntityChange::CreateDelete(storage_key, syncer::EntityData());
}

sync_pb::EntityMetadata CreateMetadata(
    CollaborationId collaboration_id,
    const SharedAttribution& shared_attribution) {
  sync_pb::EntityMetadata metadata;
  // Other metadata fields are not used in these tests.
  metadata.mutable_collaboration()->set_collaboration_id(
      std::move(collaboration_id.value()));
  metadata.mutable_collaboration()
      ->mutable_creation_attribution()
      ->set_obfuscated_gaia_id(shared_attribution.created_by.ToString());
  metadata.mutable_collaboration()
      ->mutable_last_update_attribution()
      ->set_obfuscated_gaia_id(shared_attribution.updated_by.ToString());
  return metadata;
}

class SharedTabGroupAccountDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupAccountDataSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    feature_list_.InitAndEnableFeature(syncer::kSyncSharedTabGroupAccountData);
  }

  // Creates the bridges and initializes the model. Returns true when succeeds.
  void InitializeBridgeAndModel() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(processor_, ModelReadyToSync).WillOnce(Invoke([&]() {
      quit_closure.Run();
    }));

    if (!model_) {
      model_ = std::make_unique<SavedTabGroupModel>();
    }
    bridge_ = std::make_unique<SharedTabGroupAccountDataSyncBridge>(
        std::make_unique<SyncDataTypeConfiguration>(
            processor_.CreateForwardingProcessor(),
            syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
                store_.get())),
        *model_);

    run_loop.Run();

    ASSERT_TRUE(bridge().IsInitialized());
  }

  // Tab groups and tabs need local IDs in order to be updated by the
  // account data sync bridge.
  SavedTabGroup CreateGroupWithLocalIds(
      const CollaborationId collaboration_id) {
    const LocalTabGroupID kLocalTabGroupId =
        tab_groups::test::GenerateRandomTabGroupID();

    SavedTabGroup group = test::CreateTestSavedTabGroup();
    group.SetCollaborationId(collaboration_id);
    group.SetLocalGroupId(kLocalTabGroupId);

    for (size_t i = 0; i < group.saved_tabs().size(); i++) {
      group.saved_tabs()[i].SetLocalTabID(i);
    }

    return group;
  }

  size_t GetNumEntriesInStore(bool is_tab_details) {
    std::map<std::string, sync_pb::SharedTabGroupAccountDataSpecifics> data =
        syncer::DataTypeStoreTestUtil::ReadAllDataAsProtoAndWait<
            sync_pb::SharedTabGroupAccountDataSpecifics>(*store_);

    size_t size = 0;
    for (const auto& [storage_key, specifics] : data) {
      if (is_tab_details && specifics.has_shared_tab_details()) {
        ++size;
      }
      if (!is_tab_details && specifics.has_shared_tab_group_details()) {
        ++size;
      }
    }

    return size;
  }

  size_t GetNumTabDetailsInStore() {
    return GetNumEntriesInStore(/*is_tab_details=*/true);
  }

  size_t GetNumTabGroupDetailsInStore() {
    return GetNumEntriesInStore(/*is_tab_details=*/false);
  }

  // Cleans up the bridge and the model, used to simulate browser restart.
  void StoreMetadataAndReset() {
    CHECK(model_);
    StoreSharedSyncMetadataBasedOnModel();
    bridge_.reset();
    model_.reset();
  }

  // Stores sync metadata for the shared tab groups from the current model. This
  // is helpful to verify storing data across browser restarts.
  void StoreSharedSyncMetadataBasedOnModel() {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
        store().CreateWriteBatch();
    syncer::MetadataChangeList* metadata_change_list =
        write_batch->GetMetadataChangeList();
    for (const SavedTabGroup* group : model().GetSharedTabGroupsOnly()) {
      CHECK(group->collaboration_id().has_value());
      for (const SavedTabGroupTab& tab : group->saved_tabs()) {
        metadata_change_list->UpdateMetadata(
            CreateClientTagForSharedTab(*group, tab),
            CreateMetadata(group->collaboration_id().value(),
                           tab.shared_attribution()));
      }
    }
    store().CommitWriteBatch(std::move(write_batch), base::DoNothing());
  }

  SharedTabGroupAccountDataSyncBridge& bridge() { return *bridge_; }
  syncer::DataTypeStore& store() { return *store_; }
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() {
    return processor_;
  }
  const testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() const {
    return processor_;
  }
  SavedTabGroupModel& model() { return *model_; }

 protected:
  // In memory data type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  std::unique_ptr<SharedTabGroupAccountDataSyncBridge> bridge_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SharedTabGroupAccountDataSyncBridgeTest, ShouldReturnClientTag) {
  const CollaborationId kCollaborationId("collaboration");
  SavedTabGroup group = CreateGroupWithLocalIds(kCollaborationId);

  InitializeBridgeAndModel();

  EXPECT_TRUE(bridge().SupportsGetClientTag());
  sync_pb::SharedTabGroupAccountDataSpecifics shared_tab_details =
      CreateTabGroupAccountSpecifics(kCollaborationId, group.saved_tabs().at(0),
                                     base::Time::Now());
  EXPECT_EQ(
      bridge().GetClientTag(CreateEntityData(shared_tab_details)),
      shared_tab_details.guid() + "|" + shared_tab_details.collaboration_id());
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest, ShouldCheckValidEntities) {
  const CollaborationId kCollaborationId("collaboration");
  SavedTabGroup group = CreateGroupWithLocalIds(kCollaborationId);

  InitializeBridgeAndModel();

  base::Time last_seen_time = base::Time::Now();
  EXPECT_TRUE(bridge().IsEntityDataValid(CreateEntityData(
      CreateTabGroupAccountSpecifics(kCollaborationId, group.saved_tabs().at(0),
                                     last_seen_time),
      base::Time::Now())));
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest, ShouldResolveConflicts) {
  const CollaborationId kCollaborationId("collaboration");
  SavedTabGroup group = CreateGroupWithLocalIds(kCollaborationId);

  InitializeBridgeAndModel();

  sync_pb::SharedTabGroupAccountDataSpecifics specifics1 =
      CreateTabGroupAccountSpecifics(
          kCollaborationId, group.saved_tabs().front(), base::Time::Now());
  // Create specifics for a previous change.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics2 =
      CreateTabGroupAccountSpecifics(kCollaborationId, group.saved_tabs().at(1),
                                     base::Time::Now() - base::Seconds(5));

  // Give specifics1 to sync bridge.
  syncer::EntityChangeList change_list1;
  change_list1.push_back(CreateAddEntityChange(specifics1));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list1));

  // Expect specifics1 to be used because its newer.
  EXPECT_EQ(syncer::ConflictResolution::kUseLocal,
            bridge().ResolveConflict(GetClientTagFromSpecifics(specifics1),
                                     CreateEntityData(specifics2)));

  sync_pb::SharedTabGroupAccountDataSpecifics specifics3 =
      CreateTabGroupAccountSpecifics(
          kCollaborationId, group.saved_tabs().front(), base::Time::Now());
  // Create specifics for a future change.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics4 =
      CreateTabGroupAccountSpecifics(kCollaborationId, group.saved_tabs().at(1),
                                     base::Time::Now() + base::Seconds(42));

  // Give specifics3 to sync bridge.
  syncer::EntityChangeList change_list2;
  change_list2.push_back(CreateAddEntityChange(specifics3));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list2));

  // Expect specifics4 to be used because its newer.
  EXPECT_EQ(syncer::ConflictResolution::kUseRemote,
            bridge().ResolveConflict(GetClientTagFromSpecifics(specifics3),
                                     CreateEntityData(specifics4)));

  base::Time now = base::Time::Now();
  sync_pb::SharedTabGroupAccountDataSpecifics specifics5 =
      CreateTabGroupAccountSpecifics(kCollaborationId,
                                     group.saved_tabs().front(), now);
  // Create specifics for the same time.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics6 =
      CreateTabGroupAccountSpecifics(kCollaborationId, group.saved_tabs().at(1),
                                     now);

  // Give specifics5 to sync bridge.
  syncer::EntityChangeList change_list3;
  change_list3.push_back(CreateAddEntityChange(specifics5));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list3));

  // Expect changes to match.
  EXPECT_EQ(syncer::ConflictResolution::kChangesMatch,
            bridge().ResolveConflict(GetClientTagFromSpecifics(specifics5),
                                     CreateEntityData(specifics6)));
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldAddDataAtIncrementalUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& created_group_id = created_group.saved_guid();

  const SavedTabGroupTab& created_tab1 = created_group.saved_tabs().front();
  const SavedTabGroupTab& created_tab2 = created_group.saved_tabs().at(1);
  const base::Uuid& created_tab_id1 = created_tab1.saved_tab_guid();
  const base::Uuid& created_tab_id2 = created_tab2.saved_tab_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  EXPECT_EQ(model().Count(), 1);
  EXPECT_TRUE(model().Contains(created_group_id));
  EXPECT_FALSE(created_tab1.last_seen_time().has_value());
  EXPECT_FALSE(created_tab2.last_seen_time().has_value());

  base::Time last_seen_time1 = base::Time::Now();
  base::Time last_seen_time2 = base::Time::Now();
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab1, last_seen_time1)));
  change_list.push_back(CreateUpdateEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab2, last_seen_time2)));

  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));

  // Get the group and tab after the model has been updated.
  const SavedTabGroup* group = model().Get(created_group_id);
  const SavedTabGroupTab* tab1 = group->GetTab(created_tab_id1);
  const SavedTabGroupTab* tab2 = group->GetTab(created_tab_id2);

  EXPECT_TRUE(tab1->last_seen_time().has_value());
  EXPECT_EQ(tab1->last_seen_time(), last_seen_time1);
  EXPECT_TRUE(tab2->last_seen_time().has_value());
  EXPECT_EQ(tab2->last_seen_time(), last_seen_time2);
  EXPECT_EQ(GetNumTabDetailsInStore(), 2u);
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldAddUpdateLastSeenTimestamp) {
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& created_group_id = created_group.saved_guid();

  const SavedTabGroupTab& created_tab = created_group.saved_tabs().front();
  const base::Uuid& created_tab_id = created_tab.saved_tab_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  EXPECT_EQ(model().Count(), 1);
  EXPECT_TRUE(model().Contains(created_group_id));
  EXPECT_FALSE(created_tab.last_seen_time().has_value());

  base::Time last_seen_time1 = base::Time::Now();
  syncer::EntityChangeList change_list1;
  change_list1.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab, last_seen_time1)));

  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list1));

  {
    // Get the group and tab after the model has been updated.
    const SavedTabGroup* group = model().Get(created_group_id);
    const SavedTabGroupTab* tab = group->GetTab(created_tab_id);

    EXPECT_TRUE(tab->last_seen_time().has_value());
    EXPECT_EQ(tab->last_seen_time(), last_seen_time1);
    EXPECT_EQ(GetNumTabDetailsInStore(), 1u);
  }

  base::Time last_seen_time2 = base::Time::Now() + base::Seconds(42);
  syncer::EntityChangeList change_list2;
  change_list2.push_back(
      CreateUpdateEntityChange(CreateTabGroupAccountSpecifics(
          kCollaborationId, created_tab, last_seen_time2)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list2));

  {
    // Get the group and tab after the model has been updated.
    const SavedTabGroup* group = model().Get(created_group_id);
    const SavedTabGroupTab* tab = group->GetTab(created_tab_id);

    EXPECT_TRUE(tab->last_seen_time().has_value());
    EXPECT_EQ(tab->last_seen_time(), last_seen_time2);
    EXPECT_EQ(GetNumTabDetailsInStore(), 1u);
  }
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest, ShouldDeleteDataFromSync) {
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& created_group_id = created_group.saved_guid();

  const SavedTabGroupTab& created_tab = created_group.saved_tabs().front();
  const base::Uuid& created_tab_id = created_tab.saved_tab_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  EXPECT_EQ(model().Count(), 1);
  EXPECT_TRUE(model().Contains(created_group_id));
  EXPECT_FALSE(created_tab.last_seen_time().has_value());

  base::Time last_seen_time1 = base::Time::Now();
  syncer::EntityChangeList change_list1;
  change_list1.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab, last_seen_time1)));

  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list1));

  {
    // Get the group and tab after the model has been updated.
    const SavedTabGroup* group = model().Get(created_group_id);
    const SavedTabGroupTab* tab = group->GetTab(created_tab_id);

    EXPECT_TRUE(bridge().HasSpecificsForTab(*tab));
    EXPECT_TRUE(tab->last_seen_time().has_value());
    EXPECT_EQ(tab->last_seen_time(), last_seen_time1);
    EXPECT_EQ(GetNumTabDetailsInStore(), 1u);
  }

  syncer::EntityChangeList change_list2;
  change_list2.push_back(CreateDeleteEntityChange(
      CreateClientTagForSharedTab(created_group, created_tab)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list2));

  {
    // Get the group and tab after the model has been updated.
    const SavedTabGroup* group = model().Get(created_group_id);
    const SavedTabGroupTab* tab = group->GetTab(created_tab_id);

    EXPECT_FALSE(bridge().HasSpecificsForTab(*tab));
    EXPECT_EQ(GetNumTabDetailsInStore(), 0u);
  }
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldClearStoreOnApplyDisable) {
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const SavedTabGroupTab& created_tab = created_group.saved_tabs().front();
  const base::Uuid& created_group_id = created_group.saved_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  EXPECT_EQ(model().Count(), 1);
  EXPECT_TRUE(model().Contains(created_group_id));
  EXPECT_FALSE(created_tab.last_seen_time().has_value());

  base::Time last_seen_time = base::Time::Now();
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab, last_seen_time)));

  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));
  EXPECT_EQ(GetNumTabDetailsInStore(), 1u);

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldReloadDataOnBrowserRestart) {
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const SavedTabGroupTab& created_tab = created_group.saved_tabs().front();
  const base::Uuid& created_group_id = created_group.saved_guid();
  const base::Uuid& created_tab_id = created_tab.saved_tab_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  EXPECT_EQ(model().Count(), 1);
  EXPECT_TRUE(model().Contains(created_group_id));
  EXPECT_FALSE(created_tab.last_seen_time().has_value());

  base::Time last_seen_time = base::Time::Now();
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab, last_seen_time)));

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list));

  {
    // Get the group and tab after the model has been created.
    const SavedTabGroup* group = model().Get(created_group_id);
    const SavedTabGroupTab* tab = group->GetTab(created_tab_id);

    EXPECT_TRUE(tab);
    EXPECT_TRUE(tab->last_seen_time().has_value());
    EXPECT_EQ(tab->last_seen_time(), last_seen_time);
  }

  // Mock browser restart.
  StoreMetadataAndReset();
  ASSERT_EQ(model_.get(), nullptr);

  InitializeBridgeAndModel();
  // Mock a group getting loaded from sync.
  model().LoadStoredEntries({created_group}, {created_tab});

  // When account bridge is initialized first, model data gets set.
  {
    // Get the group and tab after the model has been created.
    const SavedTabGroup* group = model().Get(created_group_id);
    const SavedTabGroupTab* tab = group->GetTab(created_tab_id);

    EXPECT_TRUE(tab);
    EXPECT_TRUE(tab->last_seen_time().has_value());
    EXPECT_EQ(tab->last_seen_time(), last_seen_time);
  }

  // Mock browser restart.
  StoreMetadataAndReset();
  ASSERT_EQ(model_.get(), nullptr);

  // Create the store and load the data so that the account bridge will
  // directly set data when loaded from disk.
  model_ = std::make_unique<SavedTabGroupModel>();
  model().LoadStoredEntries({created_group}, {created_tab});

  // Initialize bridge after model is loaded.
  InitializeBridgeAndModel();

  // When model is initialized first, model data still gets set.
  {
    // Get the group and tab after the model has been created.
    const SavedTabGroup* group = model().Get(created_group_id);
    const SavedTabGroupTab* tab = group->GetTab(created_tab_id);

    EXPECT_TRUE(tab);
    EXPECT_TRUE(tab->last_seen_time().has_value());
    EXPECT_EQ(tab->last_seen_time(), last_seen_time);
  }
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       TabDeletionsAndTimestampUpdatesWillBeSentToSync) {
  // Create a shared tab group with two tabs.
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& group_id = created_group.saved_guid();

  const SavedTabGroupTab& created_tab1 = created_group.saved_tabs()[0];
  const base::Uuid& tab_id1 = created_tab1.saved_tab_guid();
  const SavedTabGroupTab& created_tab2 = created_group.saved_tabs()[1];
  const base::Uuid& tab_id2 = created_tab2.saved_tab_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  EXPECT_EQ(model().Count(), 1);
  EXPECT_TRUE(model().Contains(group_id));
  EXPECT_FALSE(created_tab1.last_seen_time().has_value());
  EXPECT_FALSE(created_tab2.last_seen_time().has_value());

  // Send timestamp update for both tabs from sync.
  base::Time last_seen_time1 = base::Time::Now();
  base::Time last_seen_time2 = base::Time::Now() - base::Seconds(42);
  syncer::EntityChangeList change_list1;
  change_list1.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab1, last_seen_time1)));
  change_list1.push_back(CreateAddEntityChange(CreateTabGroupAccountSpecifics(
      kCollaborationId, created_tab2, last_seen_time2)));

  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(change_list1));

  // Retrieve the tabs from model after the model has been updated. Verify the
  // last seen timestamps.
  const SavedTabGroup* group = model().Get(group_id);
  const SavedTabGroupTab* tab1 = group->GetTab(tab_id1);
  const SavedTabGroupTab* tab2 = group->GetTab(tab_id2);
  const std::string storage_key1 = CreateClientTagForSharedTab(*group, *tab1);
  const std::string storage_key2 = CreateClientTagForSharedTab(*group, *tab2);

  EXPECT_TRUE(tab1->last_seen_time().has_value());
  EXPECT_EQ(tab1->last_seen_time(), last_seen_time1);
  EXPECT_TRUE(tab2->last_seen_time().has_value());
  EXPECT_EQ(tab2->last_seen_time(), last_seen_time2);
  EXPECT_EQ(GetNumTabDetailsInStore(), 2u);

  // Update the last seen timestamp for tab1 locally. The updated timestamp
  // should be sent to sync.
  base::Time last_seen_time3 = base::Time::Now() + base::Seconds(55);
  base::Time last_seen_time4 = base::Time::Now() + base::Seconds(99);

  syncer::EntityData entity_data;
  EXPECT_CALL(mock_processor(), Put(Eq(storage_key1), _, _))
      .WillOnce(SaveArgPointeeMove<1>(&entity_data));
  model().UpdateTabLastSeenTime(group_id, tab_id1, last_seen_time3,
                                TriggerSource::LOCAL);

  // Verify the written specifics.
  const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
      entity_data.specifics.shared_tab_group_account_data();
  EXPECT_EQ(kCurrentSharedTabGroupDataSpecificsProtoVersion,
            specifics.version());
  EXPECT_EQ(tab_id1.AsLowercaseString(), specifics.guid());
  EXPECT_TRUE(specifics.has_shared_tab_details());
  EXPECT_EQ(group_id.AsLowercaseString(),
            specifics.shared_tab_details().shared_tab_group_guid());
  EXPECT_EQ(last_seen_time3.ToDeltaSinceWindowsEpoch().InMicroseconds(),
            specifics.shared_tab_details().last_seen_timestamp_windows_epoch());

  // Update the last seen timestamp for tab2 from sync. The updated timestamp
  // should not be sent back to sync.
  EXPECT_CALL(mock_processor(), Put(Eq(storage_key2), _, _)).Times(0);
  model().UpdateTabLastSeenTime(group_id, tab_id2, last_seen_time4,
                                TriggerSource::REMOTE);

  ASSERT_EQ(tab1->last_seen_time(), last_seen_time3);
  EXPECT_EQ(GetNumTabDetailsInStore(), 2u);
  auto specifics1 = bridge().GetSpecificsForStorageKey(storage_key1);
  EXPECT_TRUE(specifics1.has_value());
  EXPECT_EQ(
      last_seen_time3.ToDeltaSinceWindowsEpoch().InMicroseconds(),
      specifics1->shared_tab_details().last_seen_timestamp_windows_epoch());

  ASSERT_EQ(tab2->last_seen_time(), last_seen_time4);
  EXPECT_EQ(GetNumTabDetailsInStore(), 2u);
  auto specifics2 = bridge().GetSpecificsForStorageKey(storage_key2);
  EXPECT_TRUE(specifics2.has_value());
  EXPECT_EQ(
      last_seen_time2.ToDeltaSinceWindowsEpoch().InMicroseconds(),
      specifics2->shared_tab_details().last_seen_timestamp_windows_epoch());

  // Delete the tab1 locally and tab2 from sync. The corresponding sync entries
  // for both tabs should be deleted.
  EXPECT_CALL(mock_processor(), Delete(Eq(storage_key1), _, _)).Times(1);
  EXPECT_CALL(mock_processor(), Delete(Eq(storage_key2), _, _)).Times(1);
  model().RemoveTabFromGroupLocally(group_id, tab_id1);
  model().RemoveTabFromGroupFromSync(group_id, tab_id2);
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey(storage_key1));
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey(storage_key2));
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       TabGroupDeletionLocallyWillDeleteAllTabsFromSync) {
  // Create a shared tab group with two tabs.
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& group_id = created_group.saved_guid();
  const base::Uuid& tab_id1 = created_group.saved_tabs()[0].saved_tab_guid();
  const base::Uuid& tab_id2 = created_group.saved_tabs()[1].saved_tab_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  ASSERT_EQ(model().Count(), 1);
  ASSERT_TRUE(model().Contains(group_id));
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  const std::string storage_key1 =
      CreateClientTagForSharedTab(kCollaborationId, tab_id1);
  const std::string storage_key2 =
      CreateClientTagForSharedTab(kCollaborationId, tab_id2);

  // Update the last seen timestamp for the tabs locally. The updated timestamp
  // should be sent to sync.
  EXPECT_CALL(mock_processor(), Put(Eq(storage_key1), _, _)).Times(1);
  EXPECT_CALL(mock_processor(), Put(Eq(storage_key2), _, _)).Times(1);
  model().UpdateTabLastSeenTime(group_id, tab_id1, base::Time::Now(),
                                TriggerSource::LOCAL);
  model().UpdateTabLastSeenTime(group_id, tab_id2, base::Time::Now(),
                                TriggerSource::LOCAL);
  EXPECT_EQ(GetNumTabDetailsInStore(), 2u);

  // Delete the tab group locally. The corresponding sync entries for both tabs
  // should be deleted.
  EXPECT_CALL(mock_processor(), Delete(Eq(storage_key1), _, _)).Times(1);
  EXPECT_CALL(mock_processor(), Delete(Eq(storage_key2), _, _)).Times(1);
  model().RemovedLocally(group_id);
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey(storage_key1));
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey(storage_key2));
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       TabGroupDeletionFromSyncWillDeleteAllTabsFromSync) {
  // Create a shared tab group with two tabs.
  const CollaborationId kCollaborationId("collaboration");
  const SavedTabGroup created_group = CreateGroupWithLocalIds(kCollaborationId);
  const base::Uuid& group_id = created_group.saved_guid();
  const base::Uuid& tab_id1 = created_group.saved_tabs()[0].saved_tab_guid();
  const base::Uuid& tab_id2 = created_group.saved_tabs()[1].saved_tab_guid();

  InitializeBridgeAndModel();
  model().AddedLocally(created_group);

  ASSERT_EQ(model().Count(), 1);
  ASSERT_TRUE(model().Contains(group_id));
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);

  const std::string storage_key1 =
      CreateClientTagForSharedTab(kCollaborationId, tab_id1);
  const std::string storage_key2 =
      CreateClientTagForSharedTab(kCollaborationId, tab_id2);

  // Update the last seen timestamp for the tabs locally. The updated timestamp
  // should be sent to sync.
  EXPECT_CALL(mock_processor(), Put(Eq(storage_key1), _, _)).Times(1);
  EXPECT_CALL(mock_processor(), Put(Eq(storage_key2), _, _)).Times(1);
  model().UpdateTabLastSeenTime(group_id, tab_id1, base::Time::Now(),
                                TriggerSource::LOCAL);
  model().UpdateTabLastSeenTime(group_id, tab_id2, base::Time::Now(),
                                TriggerSource::LOCAL);
  EXPECT_EQ(GetNumTabDetailsInStore(), 2u);

  // Delete the tab group from sync. The corresponding sync entries for both
  // tabs should be deleted.
  EXPECT_CALL(mock_processor(), Delete(Eq(storage_key1), _, _)).Times(1);
  EXPECT_CALL(mock_processor(), Delete(Eq(storage_key2), _, _)).Times(1);
  model().RemovedFromSync(group_id);
  EXPECT_EQ(GetNumTabDetailsInStore(), 0u);
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey(storage_key1));
  EXPECT_FALSE(bridge().GetSpecificsForStorageKey(storage_key2));
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldTrimAllSupportedFieldsFromSharedTabDetailsSpecifics) {
  InitializeBridgeAndModel();

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
    SharedTabGroupAccountDataSyncBridgeTest,
    ShouldKeepUnknownFieldsFromSharedTabAccountDataSpecifics_SharedTabDetails) {
  InitializeBridgeAndModel();

  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      extended_account_data_specifics;
  extended_account_data_specifics.set_guid("guid");
  extended_account_data_specifics.set_collaboration_id("collaboration_id");
  extended_account_data_specifics.set_update_time_windows_epoch_micros(
      1234567890);
  extended_account_data_specifics.mutable_shared_tab_details()
      ->set_shared_tab_group_guid("shared_tab_group_guid");
  extended_account_data_specifics.mutable_shared_tab_details()
      ->set_last_seen_timestamp_windows_epoch(3214567890);

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
  EXPECT_THAT(trimmed_specifics, Not(EqualsProto(sync_pb::EntitySpecifics())));

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
    SharedTabGroupAccountDataSyncBridgeTest,
    ShouldKeepUnknownFieldsFromSharedTabAccountDataSpecifics_SharedTabGroupDetails) {
  InitializeBridgeAndModel();

  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      extended_account_data_specifics;
  extended_account_data_specifics.set_guid("guid");
  extended_account_data_specifics.set_collaboration_id("collaboration_id");
  extended_account_data_specifics.set_update_time_windows_epoch_micros(
      1234567890);
  extended_account_data_specifics.mutable_shared_tab_group_details()
      ->set_pinned_position(99);

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
  EXPECT_THAT(trimmed_specifics, Not(EqualsProto(sync_pb::EntitySpecifics())));

  // Verify that deserialized proto keeps unknown fields.
  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      deserialized_extended_specifics;
  ASSERT_TRUE(deserialized_extended_specifics.ParseFromString(
      trimmed_specifics.shared_tab_group_account_data().SerializeAsString()));
  EXPECT_EQ(deserialized_extended_specifics.shared_tab_group_details()
                .extra_field_for_testing(),
            "extra_field_for_testing");
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldKeepUnknownFieldsFromSharedTabAccountDataSpecifics_TopLevel) {
  InitializeBridgeAndModel();

  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      extended_account_data_specifics;
  extended_account_data_specifics.set_guid("guid");
  extended_account_data_specifics.set_collaboration_id("collaboration_id");
  extended_account_data_specifics.set_update_time_windows_epoch_micros(
      1234567890);
  extended_account_data_specifics.mutable_shared_tab_details()
      ->set_shared_tab_group_guid("shared_tab_group_guid");
  extended_account_data_specifics.mutable_shared_tab_details()
      ->set_last_seen_timestamp_windows_epoch(3214567890);

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
  EXPECT_THAT(trimmed_specifics, Not(EqualsProto(sync_pb::EntitySpecifics())));

  // Verify that deserialized proto keeps unknown fields.
  sync_pb::test_utils::SharedTabGroupAccountDataSpecifics
      deserialized_extended_specifics;
  ASSERT_TRUE(deserialized_extended_specifics.ParseFromString(
      trimmed_specifics.shared_tab_group_account_data().SerializeAsString()));
  EXPECT_EQ(deserialized_extended_specifics.extra_field_for_testing(),
            "extra_field_for_testing");
}

}  // namespace
}  // namespace tab_groups
