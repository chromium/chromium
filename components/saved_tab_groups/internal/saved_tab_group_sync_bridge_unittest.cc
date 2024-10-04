// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service_impl.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using syncer::ConflictResolution;
using syncer::EntityData;
using testing::_;

namespace tab_groups {
namespace {

// Discard orphaned tabs after 30 days if the associated group cannot be found.
constexpr base::TimeDelta kDiscardOrphanedTabsThreshold = base::Days(30);

// Forwards SavedTabGroupModel's observer notifications to the bridge.
class ModelObserverForwarder : public SavedTabGroupModelObserver {
 public:
  ModelObserverForwarder(SavedTabGroupModel& model,
                         SavedTabGroupSyncBridge& bridge)
      : model_(model), bridge_(bridge) {
    observation_.Observe(&model);
  }

  ~ModelObserverForwarder() override = default;

  // SavedTabGroupModelObserver overrides.
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override {
    bridge_->SavedTabGroupAddedLocally(guid);
  }

  void SavedTabGroupRemovedLocally(
      const SavedTabGroup& removed_group) override {
    bridge_->SavedTabGroupRemovedLocally(removed_group);
  }

  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override {
    bridge_->SavedTabGroupUpdatedLocally(group_guid, tab_guid);
  }

  void SavedTabGroupTabMovedLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid) override {
    bridge_->SavedTabGroupTabsReorderedLocally(group_guid);
  }

  void SavedTabGroupReorderedLocally() override {
    bridge_->SavedTabGroupReorderedLocally();
  }

  void SavedTabGroupLocalIdChanged(const base::Uuid& group_guid) override {
    bridge_->SavedTabGroupLocalIdChanged(group_guid);
  }

  void SavedTabGroupLastUserInteractionTimeUpdated(
      const base::Uuid& group_guid) override {
    bridge_->SavedTabGroupLastUserInteractionTimeUpdated(group_guid);
  }

 private:
  raw_ref<SavedTabGroupModel> model_;
  raw_ref<SavedTabGroupSyncBridge> bridge_;

  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};
};

// Do not check update times for specifics as adding tabs to a group through the
// bridge will change the update times for the group object.
bool AreGroupSpecificsEqual(const sync_pb::SavedTabGroupSpecifics& sp1,
                            const sync_pb::SavedTabGroupSpecifics& sp2) {
  if (sp1.guid() != sp2.guid()) {
    return false;
  }
  if (sp1.group().title() != sp2.group().title()) {
    return false;
  }
  if (sp1.group().color() != sp2.group().color()) {
    return false;
  }
  if (sp1.group().position() != sp2.group().position()) {
    return false;
  }
  if (sp1.group().pinned_position() != sp2.group().pinned_position()) {
    return false;
  }
  if (sp1.creation_time_windows_epoch_micros() !=
      sp2.creation_time_windows_epoch_micros()) {
    return false;
  }
  return true;
}

bool AreSavedTabGroupsEqual(const SavedTabGroup& group1,
                            const SavedTabGroup& group2) {
  return AreGroupSpecificsEqual(
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(group1),
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(group2));
}

bool AreTabSpecificsEqual(const sync_pb::SavedTabGroupSpecifics& sp1,
                          const sync_pb::SavedTabGroupSpecifics& sp2) {
  if (sp1.guid() != sp2.guid()) {
    return false;
  }
  if (sp1.tab().url() != sp2.tab().url()) {
    return false;
  }
  if (sp1.tab().title() != sp2.tab().title()) {
    return false;
  }
  if (sp1.tab().group_guid() != sp2.tab().group_guid()) {
    return false;
  }
  if (sp1.creation_time_windows_epoch_micros() !=
      sp2.creation_time_windows_epoch_micros()) {
    return false;
  }
  return true;
}

bool AreSavedTabGroupTabsEqual(const SavedTabGroupTab& tab1,
                               const SavedTabGroupTab& tab2) {
  return AreTabSpecificsEqual(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(tab1),
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(tab2));
}

syncer::EntityData CreateEntityData(sync_pb::SavedTabGroupSpecifics specific) {
  syncer::EntityData entity_data;
  entity_data.name = specific.guid();
  entity_data.specifics.mutable_saved_tab_group()->Swap(&specific);
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateEntityChange(
    sync_pb::SavedTabGroupSpecifics specific,
    syncer::EntityChange::ChangeType change_type) {
  std::string guid = specific.guid();

  switch (change_type) {
    case syncer::EntityChange::ACTION_ADD:
      return syncer::EntityChange::CreateAdd(guid, CreateEntityData(specific));
    case syncer::EntityChange::ACTION_UPDATE:
      return syncer::EntityChange::CreateUpdate(guid,
                                                CreateEntityData(specific));
    case syncer::EntityChange::ACTION_DELETE:
      return syncer::EntityChange::CreateDelete(guid);
  }
}

syncer::EntityChangeList CreateEntityChangeListFromGroup(
    const SavedTabGroup& group,
    syncer::EntityChange::ChangeType change_type) {
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(group),
      change_type));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    entity_change_list.push_back(CreateEntityChange(
        SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(tab),
        change_type));
  }

  return entity_change_list;
}

}  // anonymous namespace

// // Verifies the sync bridge correctly passes/merges data in to the model.
class SavedTabGroupSyncBridgeTest : public ::testing::Test {
 public:
  SavedTabGroupSyncBridgeTest()
      : sync_bridge_model_wrapper_(
            syncer::SAVED_TAB_GROUP,
            &saved_tab_group_model_,
            base::BindOnce(&SavedTabGroupModel::LoadStoredEntries,
                           base::Unretained(&saved_tab_group_model_))),
        store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}
  ~SavedTabGroupSyncBridgeTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavedTabGroupSpecificsToDataMigration, false);
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<SavedTabGroupSyncBridge>(
        &sync_bridge_model_wrapper_,
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), &pref_service_);
    observer_forwarder_ = std::make_unique<ModelObserverForwarder>(
        saved_tab_group_model_, *bridge_);
    task_environment_.RunUntilIdle();
  }

  void VerifyEntriesCount(size_t expected_count) {
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
    store_->ReadAllData(base::BindLambdaForTesting(
        [&](const std::optional<syncer::ModelError>& error,
            std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
          entries = std::move(data);
        }));
    task_environment_.RunUntilIdle();

    ASSERT_TRUE(entries);
    EXPECT_EQ(expected_count, entries->size());
  }

  base::test::TaskEnvironment task_environment_;
  SavedTabGroupModel saved_tab_group_model_;
  SyncBridgeTabGroupModelWrapper sync_bridge_model_wrapper_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SavedTabGroupSyncBridge> bridge_;
  std::unique_ptr<ModelObserverForwarder> observer_forwarder_;
};

// Verify that when we add data into the sync bridge the SavedTabGroupModel will
// reflect those changes.
TEST_F(SavedTabGroupSyncBridgeTest, MergeFullSyncData) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {}, 0);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  // Note: Here the change type does not matter. The initial merge will add
  // all elements in the change list into the model resolving any conflicts if
  // necessary.
  bridge_->MergeFullSyncData(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  // Ensure all data passed by the bridge is the same.
  EXPECT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  const SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  EXPECT_EQ(group_from_model->saved_tabs().size(), 2u);
  EXPECT_TRUE(AreSavedTabGroupsEqual(group, *group_from_model));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreSavedTabGroupTabsEqual(
        tab, *group_from_model->GetTab(tab.saved_tab_guid())));
  }
}

TEST_F(SavedTabGroupSyncBridgeTest, ConflictResolutionForTabGroup) {
  ASSERT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title`", tab_groups::TabGroupColorId::kCyan, {},
                      1);
  SavedTabGroupTab tab(GURL("https://website1.com"), u"Website Title1",
                       group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab);
  base::Uuid group_id = group.saved_guid();
  saved_tab_group_model_.Add(std::move(group));
  ASSERT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);
  const SavedTabGroup* group_from_model = saved_tab_group_model_.Get(group_id);

  sync_pb::SavedTabGroupSpecifics group_specific =
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(
          *group_from_model);
  syncer::EntityData remote_data = CreateEntityData(group_specific);
  ASSERT_FALSE(remote_data.is_deleted());

  // Remote is old.
  group_specific.set_update_time_windows_epoch_micros(
      group_specific.update_time_windows_epoch_micros() - 10);
  remote_data = CreateEntityData(group_specific);
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              testing::Eq(syncer::ConflictResolution::kUseLocal));

  // Remote is more recent.
  group_specific.set_update_time_windows_epoch_micros(
      group_specific.update_time_windows_epoch_micros() + 10);
  remote_data = CreateEntityData(group_specific);
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              testing::Eq(syncer::ConflictResolution::kUseRemote));

  // Remote is deleted.
  syncer::EntityData remote_data2;
  remote_data2.name = group_specific.guid();
  ASSERT_TRUE(remote_data2.is_deleted());
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data2),
              testing::Eq(syncer::ConflictResolution::kUseLocal));

  // Local doesn't exist.
  saved_tab_group_model_.Remove(group_id);
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 0u);
  remote_data = CreateEntityData(group_specific);
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              testing::Eq(syncer::ConflictResolution::kUseRemote));
}

TEST_F(SavedTabGroupSyncBridgeTest, ConflictResolutionForTab) {
  ASSERT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title`", tab_groups::TabGroupColorId::kCyan, {},
                      1);
  SavedTabGroupTab tab(GURL("https://website1.com"), u"Website Title1",
                       group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab);
  base::Uuid group_id = group.saved_guid();
  base::Uuid tab_id = tab.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);
  const SavedTabGroup* group_from_model = saved_tab_group_model_.Get(group_id);

  const SavedTabGroupTab* tab_from_model = group_from_model->GetTab(tab_id);
  ASSERT_TRUE(tab_from_model);
  sync_pb::SavedTabGroupSpecifics tab_specific =
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(
          *tab_from_model);

  // Remote is old.
  tab_specific.set_update_time_windows_epoch_micros(
      tab_specific.update_time_windows_epoch_micros() - 10);
  syncer::EntityData remote_data = CreateEntityData(tab_specific);
  ASSERT_FALSE(remote_data.is_deleted());
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              testing::Eq(syncer::ConflictResolution::kUseLocal));

  // Remote is more recent.
  tab_specific.set_update_time_windows_epoch_micros(
      tab_specific.update_time_windows_epoch_micros() + 10);
  remote_data = CreateEntityData(tab_specific);
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              testing::Eq(syncer::ConflictResolution::kUseRemote));

  // Remote is deleted.
  syncer::EntityData remote_data2;
  remote_data2.name = tab_specific.guid();
  ASSERT_TRUE(remote_data2.is_deleted());
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data2),
              testing::Eq(syncer::ConflictResolution::kUseLocal));

  // Local doesn't exist.
  saved_tab_group_model_.Remove(group_id);
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 0u);
  remote_data = CreateEntityData(tab_specific);
  EXPECT_THAT(bridge_->ResolveConflict("storagekey1", remote_data),
              testing::Eq(syncer::ConflictResolution::kUseRemote));
}

// Verify merging with preexisting data in the model merges the correct
// elements.
TEST_F(SavedTabGroupSyncBridgeTest, MergeFullSyncDataWithExistingData) {
  // Force the cache guid to return the same value as the groups, reflecting
  // that the processor cache guid is the "creator".
  std::string cache_guid = "cache_guid";
  ON_CALL(processor_, TrackedCacheGuid())
      .WillByDefault(testing::Return(cache_guid));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt, /*saved_guid=*/std::nullopt,
                      /*local_group_guid=*/std::nullopt,
                      /*creator_cache_guid=*/cache_guid);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  base::Time group_creation_time = group.creation_time_windows_epoch_micros();
  base::Time tab_1_creation_time = tab_1.creation_time_windows_epoch_micros();
  base::Time tab_2_creation_time = tab_2.creation_time_windows_epoch_micros();

  saved_tab_group_model_.Add(std::move(group));

  const SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group_guid);

  // Create an updated version of `group` using the same creation time and 1
  // less tab.
  SavedTabGroup updated_group(
      u"New Title", tab_groups::TabGroupColorId::kPink, {}, /*position=*/0,
      /*saved_guid=*/group_guid,
      /*local_group_guid=*/std::nullopt, cache_guid, cache_guid,
      /*created_before_syncing_tab_groups=*/false, group_creation_time);
  SavedTabGroupTab updated_tab_1(GURL("https://support.google.com"), u"Support",
                                 group_guid, /*position=*/0, tab_1_guid,
                                 std::nullopt, cache_guid, cache_guid,
                                 tab_1_creation_time);
  updated_group.AddTabLocally(updated_tab_1);

  syncer::EntityChangeList entity_change_list = CreateEntityChangeListFromGroup(
      updated_group, syncer::EntityChange::ChangeType::ACTION_UPDATE);

  // Ensure the updated data is eligible to be merged.
  EXPECT_TRUE(group_from_model->RemoteGroupHasMoreRecentUpdates(
      updated_group.update_time_windows_epoch_micros()));
  EXPECT_TRUE(
      group_from_model->GetTab(tab_1_guid)->ShouldMergeTab(updated_tab_1));

  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(entity_change_list));

  // Ensure that tab 1 and 2 are still in the group. Data can only be removed
  // when ApplyIncrementalSyncChanges is called.
  EXPECT_EQ(group_from_model->saved_tabs().size(), 2u);
  EXPECT_TRUE(group_from_model->ContainsTab(tab_1_guid));
  EXPECT_TRUE(group_from_model->ContainsTab(tab_2_guid));

  // Ensure tab_2 was left untouched.
  SavedTabGroupTab tab_2_replica(
      GURL("https://google.com"), u"Google", group_guid, /*position=*/1,
      tab_2_guid, std::nullopt, cache_guid, cache_guid, tab_2_creation_time);
  EXPECT_TRUE(AreSavedTabGroupTabsEqual(tab_2_replica,
                                        *group_from_model->GetTab(tab_2_guid)));

  // Ensure the updated group and tab have been merged into the original group
  // in the model.
  EXPECT_TRUE(AreSavedTabGroupsEqual(*group_from_model, updated_group));
  EXPECT_TRUE(AreSavedTabGroupTabsEqual(updated_tab_1,
                                        *group_from_model->GetTab(tab_1_guid)));
}

// Verify that on sign-out, the groups created before sign-in are locally
// deleted.
TEST_F(SavedTabGroupSyncBridgeTest,
       DisableSyncLocallyRemovesGroupsCreatedBeforeSignIn) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  // Create two groups: group1 before sign in and group2 after sign in.
  SavedTabGroup group1(u"Test Title1", tab_groups::TabGroupColorId::kBlue, {},
                       0, std::nullopt, std::nullopt, std::nullopt,
                       std::nullopt,
                       /*created_before_syncing_tab_groups=*/true);
  SavedTabGroupTab tab_1(GURL("https://website1.com"), u"Website Title1",
                         group1.saved_guid(), /*position=*/std::nullopt);
  group1.AddTabLocally(tab_1);
  group1.SetCreatorCacheGuid("cache_guid_local");

  SavedTabGroup group2(u"Test Title2", tab_groups::TabGroupColorId::kCyan, {},
                       1, std::nullopt, std::nullopt, std::nullopt,
                       std::nullopt,
                       /*created_before_syncing_tab_groups=*/false);
  SavedTabGroupTab tab_2(GURL("https://website2.com"), u"Website Title2",
                         group2.saved_guid(), /*position=*/std::nullopt);
  group2.AddTabLocally(tab_2);
  group2.SetCreatorCacheGuid("cache_guid_local");

  base::Uuid group_id1 = group1.saved_guid();
  base::Uuid group_id2 = group2.saved_guid();
  saved_tab_group_model_.Add(std::move(group1));
  saved_tab_group_model_.Add(std::move(group2));
  VerifyEntriesCount(4u);

  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 2u);

  const SavedTabGroup* group1_from_model =
      saved_tab_group_model_.Get(group_id1);
  EXPECT_EQ(group1_from_model->saved_tabs().size(), 1u);

  const SavedTabGroup* group2_from_model =
      saved_tab_group_model_.Get(group_id2);
  EXPECT_EQ(group2_from_model->saved_tabs().size(), 1u);

  EXPECT_TRUE(group1_from_model->created_before_syncing_tab_groups());
  EXPECT_FALSE(group2_from_model->created_before_syncing_tab_groups());
  EXPECT_EQ("cache_guid_local", group1_from_model->creator_cache_guid());
  EXPECT_EQ("cache_guid_local", group2_from_model->creator_cache_guid());

  // Disable sync. Expect group 2 to be removed from model, and group 1 should
  // still be in the model. None of them should be deleted from sync.
  EXPECT_CALL(processor_, Delete(_, _, _)).Times(0);
  bridge_->ApplyDisableSyncChanges(bridge_->CreateMetadataChangeList());
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);
  EXPECT_TRUE(saved_tab_group_model_.Contains(group_id1));
  EXPECT_FALSE(saved_tab_group_model_.Contains(group_id2));

  // DB should have only the two entries from group 1.
  VerifyEntriesCount(2u);

  group1_from_model = saved_tab_group_model_.Get(group_id1);
  group2_from_model = saved_tab_group_model_.Get(group_id2);
  EXPECT_EQ("cache_guid_local", group1_from_model->creator_cache_guid());
}

// Verify orphaned tabs (tabs missing their group) are added into the correct
// group in the model once the group arrives.
TEST_F(SavedTabGroupSyncBridgeTest, OrphanedTabAddedIntoGroupWhenFound) {
  // Merge an orphaned tab. Then merge its missing group. This aims to
  // simulate data spread out over multiple changes.
  base::Uuid orphaned_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                orphaned_guid, /*position=*/0);

  syncer::EntityChangeList orphaned_tab_change_list;
  orphaned_tab_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(orphaned_tab),
      syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(orphaned_tab_change_list));

  EXPECT_FALSE(
      saved_tab_group_model_.Contains(orphaned_tab.saved_group_guid()));
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup missing_group(u"New Group Title",
                              tab_groups::TabGroupColorId::kOrange, {},
                              /*position=*/0, orphaned_guid);
  syncer::EntityChangeList missing_group_change_list;
  missing_group_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(missing_group),
      syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(missing_group_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  const SavedTabGroup* orphaned_group_from_model =
      saved_tab_group_model_.Get(orphaned_guid);

  EXPECT_EQ(orphaned_group_from_model->saved_tabs().size(), 1u);
  EXPECT_TRUE(
      orphaned_group_from_model->ContainsTab(orphaned_tab.saved_tab_guid()));
  EXPECT_TRUE(
      AreSavedTabGroupsEqual(missing_group, *orphaned_group_from_model));
  EXPECT_TRUE(AreSavedTabGroupTabsEqual(
      orphaned_tab,
      *orphaned_group_from_model->GetTab(orphaned_tab.saved_tab_guid())));
}

// Verify orphaned tabs (tabs missing their group) that have not been updated
// for 30 days are discarded and not added into the model.
TEST_F(SavedTabGroupSyncBridgeTest, OprhanedTabDiscardedAfter30Days) {
  // Merge an orphaned tab. Then merge its missing group. This aims to
  // simulate data spread out over multiple changes.
  base::Uuid orphaned_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                orphaned_guid, /*position=*/0);
  orphaned_tab.SetUpdateTimeWindowsEpochMicros(base::Time::Now() -
                                               kDiscardOrphanedTabsThreshold);

  syncer::EntityChangeList orphaned_tab_change_list;
  orphaned_tab_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(orphaned_tab),
      syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(orphaned_tab_change_list));

  EXPECT_FALSE(
      saved_tab_group_model_.Contains(orphaned_tab.saved_group_guid()));
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup missing_group(u"New Group Title",
                              tab_groups::TabGroupColorId::kOrange, {},
                              /*position=*/0, orphaned_guid);
  syncer::EntityChangeList missing_group_change_list;
  missing_group_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(missing_group),
      syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(missing_group_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  const SavedTabGroup* orphaned_group_from_model =
      saved_tab_group_model_.Get(orphaned_guid);
  EXPECT_TRUE(orphaned_group_from_model->saved_tabs().empty());
  EXPECT_FALSE(
      orphaned_group_from_model->ContainsTab(orphaned_tab.saved_tab_guid()));
}

// Verify orphaned tabs (tabs missing their group) that have not been updated
// for 30 days and have a group are not discarded.
TEST_F(SavedTabGroupSyncBridgeTest, OprhanedTabGroupFoundAfter30Days) {
  // Merge an orphaned tab. Then merge its missing group. This aims to
  // simulate data spread out over multiple changes.
  base::Uuid orphaned_guid = base::Uuid::GenerateRandomV4();

  SavedTabGroup missing_group(u"New Group Title",
                              tab_groups::TabGroupColorId::kOrange, {},
                              /*position=*/0, orphaned_guid);
  syncer::EntityChangeList missing_group_change_list;
  missing_group_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(missing_group),
      syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(missing_group_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                orphaned_guid, /*position=*/0);
  orphaned_tab.SetUpdateTimeWindowsEpochMicros(base::Time::Now() -
                                               kDiscardOrphanedTabsThreshold);
  syncer::EntityChangeList orphaned_tab_change_list;
  orphaned_tab_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(orphaned_tab),
      syncer::EntityChange::ChangeType::ACTION_ADD));

  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(orphaned_tab_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  const SavedTabGroup* orphaned_group_from_model =
      saved_tab_group_model_.Get(orphaned_guid);
  EXPECT_EQ(orphaned_group_from_model->saved_tabs().size(), 1u);
  EXPECT_TRUE(
      orphaned_group_from_model->ContainsTab(orphaned_tab.saved_tab_guid()));
  EXPECT_TRUE(
      AreSavedTabGroupsEqual(missing_group, *orphaned_group_from_model));
  EXPECT_TRUE(AreSavedTabGroupTabsEqual(
      orphaned_tab,
      *orphaned_group_from_model->GetTab(orphaned_tab.saved_tab_guid())));
}

// Verify that when we add data into the sync bridge the SavedTabGroupModel
// will reflect those changes.
TEST_F(SavedTabGroupSyncBridgeTest, AddSyncData) {
  syncer::EntityChangeList empty_change_list;
  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(empty_change_list));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/0);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.SetCreatorCacheGuid("remote_cache_guid");
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);
  group.SetCreatorCacheGuid("remote_cache_guid");

  bridge_->ApplyIncrementalSyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  // Ensure all data passed by the bridge is the same.
  ASSERT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  const SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  EXPECT_TRUE(AreSavedTabGroupsEqual(group, *group_from_model));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreSavedTabGroupTabsEqual(
        tab, *group_from_model->GetTab(tab.saved_tab_guid())));
  }

  // Ensure a tab added to an existing group in the bridge is added into the
  // model correctly.
  SavedTabGroupTab additional_tab(GURL("https://maps.google.com"), u"Maps",
                                  group.saved_guid(), /*position=*/2);

  // Orphaned tabs are tabs that do not have a respective group stored in the
  // model. As such, these tabs are kept in local storage but not the model.
  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                base::Uuid::GenerateRandomV4(), /*position=*/0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(
          additional_tab),
      syncer::EntityChange::ChangeType::ACTION_ADD));
  entity_change_list.push_back((CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(orphaned_tab),
      syncer::EntityChange::ChangeType::ACTION_ADD)));

  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(entity_change_list));

  ASSERT_TRUE(group_from_model->ContainsTab(additional_tab.saved_tab_guid()));
  EXPECT_EQ(group_from_model->saved_tabs().size(), 3u);
  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreSavedTabGroupTabsEqual(
        tab, *group_from_model->GetTab(tab.saved_tab_guid())));
  }
}

// Verify that ACTION_UPDATE performs the same as ACTION_ADD initially and that
// the model reflects the updated group data after subsequent calls.
TEST_F(SavedTabGroupSyncBridgeTest, UpdateSyncData) {
  syncer::EntityChangeList empty_change_list;
  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(empty_change_list));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);
  group.SetPosition(0);

  bridge_->ApplyIncrementalSyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  const SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  group.SetTitle(u"A new title");
  group.SetColor(tab_groups::TabGroupColorId::kRed);
  group.saved_tabs()[0].SetURL(GURL("https://youtube.com"));

  bridge_->ApplyIncrementalSyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_UPDATE));

  EXPECT_TRUE(AreSavedTabGroupsEqual(group, *group_from_model));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreSavedTabGroupTabsEqual(
        tab, *group_from_model->GetTab(tab.saved_tab_guid())));
  }
}

// Verify that the correct elements are removed when ACTION_DELETE is called.
TEST_F(SavedTabGroupSyncBridgeTest, DeleteSyncData) {
  syncer::EntityChangeList empty_change_list;
  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(empty_change_list));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/0);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  EXPECT_EQ(group.saved_tabs().size(), 2u);

  bridge_->ApplyIncrementalSyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  ASSERT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  const SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  // Ensure a deleted tab is deleted from the group correctly in the model.
  base::Uuid tab_to_remove = group.saved_tabs()[0].saved_tab_guid();

  syncer::EntityChangeList delete_tab_change_list;
  delete_tab_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(
          group.saved_tabs()[0]),
      syncer::EntityChange::ChangeType::ACTION_DELETE));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(delete_tab_change_list));

  EXPECT_EQ(group_from_model->saved_tabs().size(), 1u);
  EXPECT_TRUE(AreSavedTabGroupTabsEqual(group.saved_tabs()[1],
                                        group_from_model->saved_tabs()[0]));
  EXPECT_FALSE(group_from_model->ContainsTab(tab_to_remove));

  // Ensure deleting a group deletes all the tabs in the group as well.
  syncer::EntityChangeList delete_group_change_list;

  delete_group_change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(group),
      syncer::EntityChange::ChangeType::ACTION_DELETE));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(delete_group_change_list));

  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 0u);
  EXPECT_FALSE(saved_tab_group_model_.Contains(group.saved_guid()));
}

// Verify that the deleted elements are processed last. We process deleted
// elements last for consistency since the ordering of messages is not
// guaranteed.
TEST_F(SavedTabGroupSyncBridgeTest, DeleteSyncDataProcessedLast) {
  syncer::EntityChangeList empty_change_list;
  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                             std::move(empty_change_list));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/0);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_3(GURL("https://youtube.com"), u"Youtube",
                         group.saved_guid(), /*position=*/2);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);
  EXPECT_EQ(group.saved_tabs().size(), 2u);

  bridge_->ApplyIncrementalSyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  ASSERT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  const SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  // Ensure the deleted tabs are removed from the group correctly.
  base::Uuid removed_tab_1 = group.saved_tabs()[0].saved_tab_guid();
  base::Uuid removed_tab_2 = group.saved_tabs()[1].saved_tab_guid();

  // Remove both tabs in the group first, then add the new tab.
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(
          group.saved_tabs()[0]),
      syncer::EntityChange::ChangeType::ACTION_DELETE));
  change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(
          group.saved_tabs()[1]),
      syncer::EntityChange::ChangeType::ACTION_DELETE));
  change_list.push_back(CreateEntityChange(
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(tab_3),
      syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                       std::move(change_list));

  // The group should still exist with only `tab_3`.
  ASSERT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  EXPECT_EQ(group_from_model->saved_tabs().size(), 1u);
  EXPECT_TRUE(
      AreSavedTabGroupTabsEqual(tab_3, group_from_model->saved_tabs()[0]));

  EXPECT_FALSE(group_from_model->ContainsTab(removed_tab_1));
  EXPECT_FALSE(group_from_model->ContainsTab(removed_tab_2));
  EXPECT_TRUE(group_from_model->ContainsTab(tab_3.saved_tab_guid()));
}

// Verify that locally added groups call add all group data to the processor.
TEST_F(SavedTabGroupSyncBridgeTest, AddGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();

  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _));

  saved_tab_group_model_.Add(std::move(group));
}

// Verify that local ID change events aren't passed to the processor.
TEST_F(SavedTabGroupSyncBridgeTest, LocalIdChanged) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1);
  saved_tab_group_model_.Add(group);

  // Local ID change events on tabs or groups shouldn't propagate to the
  // processor.
  EXPECT_CALL(processor_, Put(_, _, _)).Times(0);
  // Open the group.
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  tab_1.SetLocalTabID(test::GenerateRandomTabID());

  // Close the group.
  group.SetLocalGroupId(std::nullopt);
  tab_1.SetLocalTabID(std::nullopt);
}

// Verify that locally removed groups removes the group from the processor
// and leaves the tabs without an associated group.
TEST_F(SavedTabGroupSyncBridgeTest, RemoveGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Delete(group_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Delete(tab_1_guid.AsLowercaseString(), _, _))
      .Times(0);
  EXPECT_CALL(processor_, Delete(tab_2_guid.AsLowercaseString(), _, _))
      .Times(0);

  saved_tab_group_model_.Remove(group_guid);

  // Verify that the orphaned tabs are still stored locally in the sync bridge.
  const std::vector<proto::SavedTabGroupData>& tabs_missing_groups =
      bridge_->GetTabsMissingGroupsForTesting();

  auto it_1 = base::ranges::find_if(
      tabs_missing_groups, [&](proto::SavedTabGroupData data) {
        return data.specifics().guid() == tab_1_guid.AsLowercaseString();
      });
  auto it_2 = base::ranges::find_if(
      tabs_missing_groups, [&](proto::SavedTabGroupData data) {
        return data.specifics().guid() == tab_2_guid.AsLowercaseString();
      });

  EXPECT_TRUE(it_1 != tabs_missing_groups.end());
  EXPECT_TRUE(it_2 != tabs_missing_groups.end());
}

// Verify that locally updated groups add all group data to the processor.
TEST_F(SavedTabGroupSyncBridgeTest, UpdateGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt,
                      /*saved_guid=*/base::Uuid::GenerateRandomV4(),
                      /*local_group_id=*/test::GenerateRandomTabGroupID());
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);

  tab_groups::TabGroupVisualData visual_data(
      u"New Title", tab_groups::TabGroupColorId::kYellow);
  saved_tab_group_model_.UpdateVisualData(group.local_group_id().value(),
                                          &visual_data);
}

// Verify duplicate tab added from sync is merged with the correct tab and not
// added again to the model.
TEST_F(SavedTabGroupSyncBridgeTest, AddTabFromSync) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_3(tab_2);
  tab_3.SetPosition(0);

  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  base::Uuid tab_3_guid = tab_3.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));
  EXPECT_CALL(processor_, Put(tab_3_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  saved_tab_group_model_.AddTabToGroupFromSync(group_guid, tab_3);

  EXPECT_EQ(tab_2_guid, tab_3_guid);
  EXPECT_EQ(
      saved_tab_group_model_.Get(group_guid)->GetTab(tab_2_guid)->position(),
      saved_tab_group_model_.Get(group_guid)->GetTab(tab_3_guid)->position());
}

// Verify that locally added tabs call put on the processor.
TEST_F(SavedTabGroupSyncBridgeTest, AddTabLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_3(GURL("https://youtube.com"), u"Youtube",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  base::Uuid tab_3_guid = tab_3.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Put(tab_3_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  saved_tab_group_model_.AddTabToGroupLocally(group_guid, tab_3);
}

// Verify that locally removed tabs remove the correct tabs from the processor.
TEST_F(SavedTabGroupSyncBridgeTest, RemoveTabLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Goole",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Delete(tab_1_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  saved_tab_group_model_.RemoveTabFromGroupLocally(group_guid, tab_1_guid);
}

// Verify that locally updated tabs update the correct tabs in the processor.
TEST_F(SavedTabGroupSyncBridgeTest, UpdateTabLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  SavedTabGroupTab updated_tab_1(group.saved_tabs()[0]);
  updated_tab_1.SetURL(GURL("https://youtube.com"));
  updated_tab_1.SetTitle(u"Youtube");

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  saved_tab_group_model_.UpdateTabInGroup(group_guid, updated_tab_1);
}

// Verify that locally reordered tabs updates all tabs in the group.
TEST_F(SavedTabGroupSyncBridgeTest, ReorderTabsInGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  SavedTabGroupTab updated_tab_1(group.saved_tabs()[0]);
  updated_tab_1.SetURL(GURL("https://youtube.com"));
  updated_tab_1.SetTitle(u"Youtube");

  base::Uuid group_guid = group.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  saved_tab_group_model_.MoveTabInGroupTo(group_guid, tab_1_guid, 1);
}

// Verify that locally reordered tabs updates all tabs in the group.
TEST_F(SavedTabGroupSyncBridgeTest, ReorderGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {},
                      /*position=*/std::nullopt);
  SavedTabGroup group_2(u"Test Title 2", tab_groups::TabGroupColorId::kRed, {},
                        /*position=*/std::nullopt);
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1).AddTabLocally(tab_2);

  SavedTabGroupTab updated_tab_1(group.saved_tabs()[0]);
  updated_tab_1.SetURL(GURL("https://youtube.com"));
  updated_tab_1.SetTitle(u"Youtube");

  base::Uuid group_guid = group.saved_guid();
  base::Uuid group_2_guid = group_2.saved_guid();
  base::Uuid tab_1_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));
  saved_tab_group_model_.Add(std::move(group_2));

  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(group_2_guid.AsLowercaseString(), _, _));

  saved_tab_group_model_.ReorderGroupLocally(group_guid, 1);
}

// Verify that pulling the cache guid works.
TEST_F(SavedTabGroupSyncBridgeTest, Group) {
  const std::string expected_guid = "cache_guid";
  ON_CALL(processor_, TrackedCacheGuid())
      .WillByDefault(testing::Return(expected_guid));

  ASSERT_TRUE(processor_.IsTrackingMetadata());
  ASSERT_EQ(processor_.TrackedCacheGuid(), expected_guid);

  std::optional<std::string> maybe_cache_guid = bridge_->GetLocalCacheGuid();
  EXPECT_EQ(maybe_cache_guid, expected_guid);
}

class SavedTabGroupSyncBridgeMigrationTest
    : public SavedTabGroupSyncBridgeTest {
 public:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavedTabGroupSpecificsToDataMigration, false);
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(processor_, TrackedCacheGuid())
        .WillByDefault(testing::Return("local_cache_guid"));
  }

  void CreateBridge(bool has_specifics_migrated) {
    pref_service_.SetBoolean(prefs::kSavedTabGroupSpecificsToDataMigration,
                             has_specifics_migrated);
    bridge_ = std::make_unique<SavedTabGroupSyncBridge>(
        &sync_bridge_model_wrapper_,
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), &pref_service_);
    task_environment_.RunUntilIdle();
  }
};

TEST_F(
    SavedTabGroupSyncBridgeMigrationTest,
    MigrateSpecificsToSavedTabGroupData_OldToNewFormat_Success_OneGroup_VerifyNewRecord) {
  // Create a SavedTabGroup and serialize in the old format.
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kBlue, {}, 0,
                      base::Uuid::GenerateRandomV4());
  sync_pb::SavedTabGroupSpecifics old_specifics =
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(group);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(old_specifics.guid(), old_specifics.SerializeAsString());
  store_->CommitWriteBatch(std::move(batch), base::DoNothing());

  // Create the bridge. That should trigger migration.
  CreateBridge(/*has_specifics_migrated=*/false);
  task_environment_.RunUntilIdle();

  // Read the migrated data from the store.
  std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
  store_->ReadAllData(base::BindLambdaForTesting(
      [&](const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
        entries = std::move(data);
      }));
  task_environment_.RunUntilIdle();

  // Verify the migrated data
  ASSERT_TRUE(entries);
  EXPECT_EQ(entries->size(), 1u);
  const syncer::DataTypeStore::Record& record = entries->at(0);
  proto::SavedTabGroupData migrated_data;
  ASSERT_TRUE(migrated_data.ParseFromString(record.value));

  EXPECT_TRUE(AreGroupSpecificsEqual(migrated_data.specifics(), old_specifics));

  // Verify that the migration pref is set to true.
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kSavedTabGroupSpecificsToDataMigration));
}

TEST_F(
    SavedTabGroupSyncBridgeMigrationTest,
    MigrateSpecificsToSavedTabGroupData_OldToNewFormat_Success_OneGroupWithOneTab) {
  // Create a SavedTabGroup with one tab and serialize in the old format.
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kBlue, {}, 0,
                      base::Uuid::GenerateRandomV4());
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), 0);
  group.AddTabLocally(tab_1);

  sync_pb::SavedTabGroupSpecifics old_specifics =
      SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(group);
  sync_pb::SavedTabGroupSpecifics old_tab_specifics =
      SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(tab_1);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(old_specifics.guid(), old_specifics.SerializeAsString());
  batch->WriteData(old_tab_specifics.guid(),
                   old_tab_specifics.SerializeAsString());
  store_->CommitWriteBatch(std::move(batch), base::DoNothing());

  // Create the bridge. That should trigger migration.
  CreateBridge(/*has_specifics_migrated=*/false);
  task_environment_.RunUntilIdle();

  // Read the migrated data from the store.
  std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
  store_->ReadAllData(base::BindLambdaForTesting(
      [&](const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
        entries = std::move(data);
      }));
  task_environment_.RunUntilIdle();

  // Verify the migrated data
  ASSERT_TRUE(entries);
  EXPECT_EQ(entries->size(), 2u);
  EXPECT_EQ(1u, saved_tab_group_model_.saved_tab_groups().size());

  // Verify the migrated data in the model.
  const SavedTabGroup* migrated_group =
      saved_tab_group_model_.Get(group.saved_guid());
  EXPECT_TRUE(migrated_group);  // The group should exist in the model

  // Compare the migrated group with the original group (excluding
  // local_group_id).
  EXPECT_EQ(migrated_group->title(), group.title());
  EXPECT_EQ(migrated_group->color(), group.color());
  EXPECT_EQ(migrated_group->position(), group.position());

  EXPECT_FALSE(migrated_group->local_group_id().has_value());

  // Verify the migrated tabs.
  const SavedTabGroupTab* migrated_tab =
      migrated_group->GetTab(tab_1.saved_tab_guid());
  EXPECT_TRUE(migrated_tab);
  EXPECT_EQ(migrated_tab->url(), tab_1.url());
  EXPECT_EQ(migrated_tab->title(), tab_1.title());

  // Verify that the migration pref is set to true.
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kSavedTabGroupSpecificsToDataMigration));
}

TEST_F(SavedTabGroupSyncBridgeMigrationTest,
       MigrateSpecificsToSavedTabGroupData_CorruptedData) {
  // 1. Create invalid data.
  std::string invalid_data = "this is not a valid protobuf";

  // 2. Write the invalid data to the DataTypeStore.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                   invalid_data);
  store_->CommitWriteBatch(std::move(batch), base::DoNothing());

  // 3. Create the bridge (triggering migration).
  CreateBridge(/*has_specifics_migrated=*/false);
  task_environment_.RunUntilIdle();

  // 4. Verify that the migration didn't crash and the model is empty.
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());
}

TEST_F(SavedTabGroupSyncBridgeMigrationTest,
       SavedTabGroupSyncBridgeMigrationTest_AlreadyMigrated) {
  // Create a SavedTabGroup and serialize in the new format.
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kBlue, {}, 0,
                      base::Uuid::GenerateRandomV4());

  proto::SavedTabGroupData group_data =
      SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(group);
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(group_data.specifics().guid(),
                   group_data.SerializeAsString());
  store_->CommitWriteBatch(std::move(batch), base::DoNothing());

  // Create the bridge with pref set to true. That should not trigger migration.
  CreateBridge(/*has_specifics_migrated=*/true);
  task_environment_.RunUntilIdle();

  // Read the migrated data from the store.
  std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
  store_->ReadAllData(base::BindLambdaForTesting(
      [&](const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
        entries = std::move(data);
      }));
  task_environment_.RunUntilIdle();

  // Verify the migrated data. It should match the original.
  ASSERT_TRUE(entries);
  EXPECT_EQ(entries->size(), 1u);
  const syncer::DataTypeStore::Record& record = entries->at(0);
  proto::SavedTabGroupData migrated_data;
  EXPECT_EQ(group_data.SerializeAsString(), record.value);
  ASSERT_TRUE(migrated_data.ParseFromString(record.value));
  EXPECT_TRUE(AreGroupSpecificsEqual(migrated_data.specifics(),
                                     group_data.specifics()));

  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kSavedTabGroupSpecificsToDataMigration));
}

TEST_F(SavedTabGroupSyncBridgeMigrationTest,
       SavedTabGroupSyncBridgeMigrationTest_NewFormatBeforeMigration) {
  // Create a SavedTabGroup and serialize in the new format.
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kBlue, {}, 0,
                      base::Uuid::GenerateRandomV4());

  proto::SavedTabGroupData group_data =
      SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(group);
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(group_data.specifics().guid(),
                   group_data.SerializeAsString());
  store_->CommitWriteBatch(std::move(batch), base::DoNothing());

  // Create the bridge with pref set to true. That should not trigger migration.
  CreateBridge(/*has_migrated=*/false);
  task_environment_.RunUntilIdle();

  // Read the migrated data from the store.
  std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
  store_->ReadAllData(base::BindLambdaForTesting(
      [&](const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
        entries = std::move(data);
      }));
  task_environment_.RunUntilIdle();

  // Verify the migrated data. It should match the original.
  ASSERT_TRUE(entries);
  EXPECT_EQ(entries->size(), 1u);
  const syncer::DataTypeStore::Record& record = entries->at(0);
  proto::SavedTabGroupData migrated_data;
  EXPECT_EQ(group_data.SerializeAsString(), record.value);
  ASSERT_TRUE(migrated_data.ParseFromString(record.value));
  EXPECT_TRUE(AreGroupSpecificsEqual(migrated_data.specifics(),
                                     group_data.specifics()));

  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kSavedTabGroupSpecificsToDataMigration));
}

TEST_F(
    SavedTabGroupSyncBridgeMigrationTest,
    MigrateSpecificsToSavedTabGroupData_AlreadyNewFormatBeforeMigration_Success_OneGroupWithOneTab) {
  // Create a SavedTabGroup with one tab and serialize in the old format.
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kBlue, {}, 0,
                      base::Uuid::GenerateRandomV4());
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid(), 0);
  group.AddTabLocally(tab_1);

  proto::SavedTabGroupData group_data =
      SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(group);
  proto::SavedTabGroupData tab_data =
      SavedTabGroupSyncBridge::SavedTabGroupTabToDataForTest(tab_1);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(group_data.specifics().guid(),
                   group_data.SerializeAsString());
  batch->WriteData(tab_data.specifics().guid(), tab_data.SerializeAsString());
  store_->CommitWriteBatch(std::move(batch), base::DoNothing());

  // Create the bridge. That should trigger migration.
  CreateBridge(/*has_migrated=*/false);
  task_environment_.RunUntilIdle();

  // Read the migrated data from the store.
  std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
  store_->ReadAllData(base::BindLambdaForTesting(
      [&](const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
        entries = std::move(data);
      }));
  task_environment_.RunUntilIdle();

  // Verify the migrated data
  ASSERT_TRUE(entries);
  EXPECT_EQ(entries->size(), 2u);
  EXPECT_EQ(1u, saved_tab_group_model_.saved_tab_groups().size());

  // Verify the migrated data in the model.
  const SavedTabGroup* migrated_group =
      saved_tab_group_model_.Get(group.saved_guid());
  EXPECT_TRUE(migrated_group);  // The group should exist in the model

  // Compare the migrated group with the original group (excluding
  // local_group_id).
  EXPECT_EQ(migrated_group->title(), group.title());
  EXPECT_EQ(migrated_group->color(), group.color());
  EXPECT_EQ(migrated_group->position(), group.position());

  EXPECT_FALSE(migrated_group->local_group_id().has_value());

  // Verify the migrated tabs.
  const SavedTabGroupTab* migrated_tab =
      migrated_group->GetTab(tab_1.saved_tab_guid());
  EXPECT_TRUE(migrated_tab);
  EXPECT_EQ(migrated_tab->url(), tab_1.url());
  EXPECT_EQ(migrated_tab->title(), tab_1.title());

  // Verify that the migration pref is set to true.
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kSavedTabGroupSpecificsToDataMigration));
}

TEST_F(SavedTabGroupSyncBridgeTest, NewlyOrphanedGroupsDontGetDestroyed) {
  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  // position must be set or the update time will be overridden during model
  // save.
  group.SetPosition(0);
  group.SetUpdateTimeWindowsEpochMicros(base::Time::Now());

  saved_tab_group_model_.Add(std::move(group));
  EXPECT_EQ(1u, saved_tab_group_model_.saved_tab_groups().size());

  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(), {});

  EXPECT_EQ(1u, saved_tab_group_model_.saved_tab_groups().size());
}

TEST_F(SavedTabGroupSyncBridgeTest, OldOrphanedGroupsGetDestroyed) {
  auto id = base::Uuid::GenerateRandomV4();
  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  // position must be set or the update time will be overridden during model
  // save.
  group.SetPosition(0);

  group.SetUpdateTimeWindowsEpochMicros(
      (base::Time::Now() - kDiscardOrphanedTabsThreshold) - base::Days(1));

  saved_tab_group_model_.Add(std::move(group));
  EXPECT_EQ(1u, saved_tab_group_model_.saved_tab_groups().size());

  bridge_->MergeFullSyncData(bridge_->CreateMetadataChangeList(), {});

  EXPECT_EQ(0u, saved_tab_group_model_.saved_tab_groups().size());
}

}  // namespace tab_groups
