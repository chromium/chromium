// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/guid.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service_impl.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using testing::_;

namespace {
constexpr base::TimeDelta discard_orphaned_tabs_threshold =
    base::Microseconds(base::Time::kMicrosecondsPerDay * 90);

// Do not check update times for specifics as adding tabs to a group through the
// bridge will change the update times for the group object.
bool AreGroupSpecificsEqual(const sync_pb::SavedTabGroupSpecifics& sp1,
                            const sync_pb::SavedTabGroupSpecifics& sp2) {
  if (sp1.guid() != sp2.guid())
    return false;
  if (sp1.group().title() != sp2.group().title())
    return false;
  if (sp1.group().color() != sp2.group().color())
    return false;
  if (sp1.group().position() != sp2.group().position())
    return false;
  if (sp1.creation_time_windows_epoch_micros() !=
      sp2.creation_time_windows_epoch_micros()) {
    return false;
  }
  return true;
}

bool AreTabSpecificsEqual(const sync_pb::SavedTabGroupSpecifics& sp1,
                          const sync_pb::SavedTabGroupSpecifics& sp2) {
  if (sp1.guid() != sp2.guid())
    return false;
  if (sp1.tab().url() != sp2.tab().url())
    return false;
  if (sp1.tab().title() != sp2.tab().title())
    return false;
  if (sp1.tab().group_guid() != sp2.tab().group_guid())
    return false;
  if (sp1.creation_time_windows_epoch_micros() !=
      sp2.creation_time_windows_epoch_micros()) {
    return false;
  }
  return true;
}

syncer::EntityData CreateEntityData(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific) {
  syncer::EntityData entity_data;
  entity_data.name = specific->guid();
  entity_data.specifics.set_allocated_saved_tab_group(specific.release());
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateEntityChange(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific,
    syncer::EntityChange::ChangeType change_type) {
  std::string guid = specific->guid();

  switch (change_type) {
    case syncer::EntityChange::ACTION_ADD:
      return syncer::EntityChange::CreateAdd(
          guid, CreateEntityData(std::move(specific)));
    case syncer::EntityChange::ACTION_UPDATE:
      return syncer::EntityChange::CreateUpdate(
          guid, CreateEntityData(std::move(specific)));
    case syncer::EntityChange::ACTION_DELETE:
      return syncer::EntityChange::CreateDelete(guid);
  }
}

syncer::EntityChangeList CreateEntityChangeListFromGroup(
    const SavedTabGroup& group,
    syncer::EntityChange::ChangeType change_type) {
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      CreateEntityChange(group.ToSpecifics(), change_type));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    entity_change_list.push_back(
        CreateEntityChange(tab.ToSpecifics(), change_type));
  }

  return entity_change_list;
}

}  // anonymous namespace

// // Verifies the sync bridge correctly passes/merges data in to the model.
class SavedTabGroupSyncBridgeTest : public ::testing::Test {
 public:
  SavedTabGroupSyncBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    InitializeBridge();
  }
  ~SavedTabGroupSyncBridgeTest() override = default;

  void InitializeBridge() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<SavedTabGroupSyncBridge>(
        &saved_tab_group_model_,
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor());
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  SavedTabGroupModel saved_tab_group_model_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<SavedTabGroupSyncBridge> bridge_;
};

// Verify that when we add data into the sync bridge the SavedTabGroupModel will
// reflect those changes.
TEST_F(SavedTabGroupSyncBridgeTest, MergeSyncData) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);
  group.SetPosition(0);

  // Note: Here the change type does not matter. The initial merge will add
  // all elements in the change list into the model resolving any conflicts if
  // necessary.
  bridge_->MergeSyncData(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  // Ensure all data passed by the bridge is the same.
  EXPECT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  EXPECT_EQ(group_from_model->saved_tabs().size(), 2u);
  EXPECT_TRUE(AreGroupSpecificsEqual(*group.ToSpecifics(),
                                     *group_from_model->ToSpecifics()));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreTabSpecificsEqual(
        *tab.ToSpecifics(),
        *group_from_model->GetTab(tab.saved_tab_guid())->ToSpecifics()));
  }
}

// Verify merging with preexisting data in the model merges the correct
// elements.
TEST_F(SavedTabGroupSyncBridgeTest, MergeSyncDataWithExistingData) {
  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  base::GUID group_guid = group.saved_guid();
  base::GUID tab_1_guid = tab_1.saved_tab_guid();
  base::GUID tab_2_guid = tab_2.saved_tab_guid();
  base::Time group_creation_time = group.creation_time_windows_epoch_micros();
  base::Time tab_1_creation_time = tab_1.creation_time_windows_epoch_micros();
  base::Time tab_2_creation_time = tab_2.creation_time_windows_epoch_micros();

  saved_tab_group_model_.Add(std::move(group));

  SavedTabGroup* group_from_model = saved_tab_group_model_.Get(group_guid);

  // Create an updated version of `group` using the same creation time and 1
  // less tab.
  SavedTabGroup updated_group(u"New Title", tab_groups::TabGroupColorId::kPink,
                              {}, group_guid, absl::nullopt, absl::nullopt,
                              group_creation_time);
  SavedTabGroupTab updated_tab_1(GURL("https://support.google.com"), u"Support",
                                 group_guid, nullptr, tab_1_guid, absl::nullopt,
                                 absl::nullopt, tab_1_creation_time);
  updated_group.AddTab(updated_tab_1);
  updated_group.SetPosition(0);

  syncer::EntityChangeList entity_change_list = CreateEntityChangeListFromGroup(
      updated_group, syncer::EntityChange::ChangeType::ACTION_UPDATE);

  // Ensure the updated data is eligible to be merged.
  EXPECT_TRUE(group_from_model->ShouldMergeGroup(*updated_group.ToSpecifics()));
  EXPECT_TRUE(group_from_model->GetTab(tab_1_guid)
                  ->ShouldMergeTab(*updated_tab_1.ToSpecifics()));

  bridge_->MergeSyncData(bridge_->CreateMetadataChangeList(),
                         std::move(entity_change_list));

  // Ensure that tab 1 and 2 are still in the group. Data can only be removed
  // when ApplySyncChanges is called.
  EXPECT_EQ(group_from_model->saved_tabs().size(), 2u);
  EXPECT_TRUE(group_from_model->ContainsTab(tab_1_guid));
  EXPECT_TRUE(group_from_model->ContainsTab(tab_2_guid));

  // Ensure tab_2 was left untouched.
  SavedTabGroupTab tab_2_replica(GURL("https://google.com"), u"Google",
                                 group_guid, nullptr, tab_2_guid, absl::nullopt,
                                 absl::nullopt, tab_2_creation_time);
  EXPECT_TRUE(AreTabSpecificsEqual(
      *tab_2_replica.ToSpecifics(),
      *group_from_model->GetTab(tab_2_guid)->ToSpecifics()));

  // Ensure the updated group and tab have been merged into the original group
  // in the model.
  EXPECT_TRUE(AreGroupSpecificsEqual(*group_from_model->ToSpecifics(),
                                     *updated_group.ToSpecifics()));
  EXPECT_TRUE(AreTabSpecificsEqual(
      *updated_tab_1.ToSpecifics(),
      *group_from_model->GetTab(tab_1_guid)->ToSpecifics()));
}

// Verify orphaned tabs (tabs missing their group) are added into the correct
// group in the model once the group arrives.
TEST_F(SavedTabGroupSyncBridgeTest, OrphanedTabAddedIntoGroupWhenFound) {
  // Merge an orphaned tab. Then merge its missing group. This aims to
  // simulate data spread out over multiple changes.
  base::GUID orphaned_guid = base::GUID::GenerateRandomV4();
  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                orphaned_guid);

  syncer::EntityChangeList orphaned_tab_change_list;
  orphaned_tab_change_list.push_back(
      CreateEntityChange(orphaned_tab.ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->MergeSyncData(bridge_->CreateMetadataChangeList(),
                         std::move(orphaned_tab_change_list));

  EXPECT_FALSE(
      saved_tab_group_model_.Contains(orphaned_tab.saved_group_guid()));
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup missing_group(u"New Group Title",
                              tab_groups::TabGroupColorId::kOrange, {},
                              orphaned_guid);
  missing_group.SetPosition(0);
  syncer::EntityChangeList missing_group_change_list;
  missing_group_change_list.push_back(
      CreateEntityChange(missing_group.ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->ApplySyncChanges(bridge_->CreateMetadataChangeList(),
                            std::move(missing_group_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  SavedTabGroup* orphaned_group_from_model =
      saved_tab_group_model_.Get(orphaned_guid);

  EXPECT_EQ(orphaned_group_from_model->saved_tabs().size(), 1u);
  EXPECT_TRUE(
      orphaned_group_from_model->ContainsTab(orphaned_tab.saved_tab_guid()));
  EXPECT_TRUE(AreGroupSpecificsEqual(
      *missing_group.ToSpecifics(), *orphaned_group_from_model->ToSpecifics()));
  EXPECT_TRUE(AreTabSpecificsEqual(
      *orphaned_tab.ToSpecifics(),
      *orphaned_group_from_model->GetTab(orphaned_tab.saved_tab_guid())
           ->ToSpecifics()));
}

// Verify orphaned tabs (tabs missing their group) that have not been updated
// for 90 days are discarded and not added into the model.
TEST_F(SavedTabGroupSyncBridgeTest, OprhanedTabDiscardedAfter90Days) {
  // Merge an orphaned tab. Then merge its missing group. This aims to
  // simulate data spread out over multiple changes.
  base::GUID orphaned_guid = base::GUID::GenerateRandomV4();
  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                orphaned_guid);
  orphaned_tab.SetUpdateTimeWindowsEpochMicros(base::Time::Now() -
                                               discard_orphaned_tabs_threshold);

  syncer::EntityChangeList orphaned_tab_change_list;
  orphaned_tab_change_list.push_back(
      CreateEntityChange(orphaned_tab.ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->MergeSyncData(bridge_->CreateMetadataChangeList(),
                         std::move(orphaned_tab_change_list));

  EXPECT_FALSE(
      saved_tab_group_model_.Contains(orphaned_tab.saved_group_guid()));
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup missing_group(u"New Group Title",
                              tab_groups::TabGroupColorId::kOrange, {},
                              orphaned_guid);
  syncer::EntityChangeList missing_group_change_list;
  missing_group_change_list.push_back(
      CreateEntityChange(missing_group.ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->ApplySyncChanges(bridge_->CreateMetadataChangeList(),
                            std::move(missing_group_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  SavedTabGroup* orphaned_group_from_model =
      saved_tab_group_model_.Get(orphaned_guid);
  EXPECT_TRUE(orphaned_group_from_model->saved_tabs().empty());
  EXPECT_FALSE(
      orphaned_group_from_model->ContainsTab(orphaned_tab.saved_tab_guid()));
}

// Verify orphaned tabs (tabs missing their group) that have not been updated
// for 90 days and have a group are not discarded.
TEST_F(SavedTabGroupSyncBridgeTest, OprhanedTabGroupFoundAfter90Days) {
  // Merge an orphaned tab. Then merge its missing group. This aims to
  // simulate data spread out over multiple changes.
  base::GUID orphaned_guid = base::GUID::GenerateRandomV4();

  SavedTabGroup missing_group(u"New Group Title",
                              tab_groups::TabGroupColorId::kOrange, {},
                              orphaned_guid);
  syncer::EntityChangeList missing_group_change_list;
  missing_group_change_list.push_back(
      CreateEntityChange(missing_group.ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_ADD));
  bridge_->MergeSyncData(bridge_->CreateMetadataChangeList(),
                         std::move(missing_group_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                orphaned_guid);
  orphaned_tab.SetUpdateTimeWindowsEpochMicros(base::Time::Now() -
                                               discard_orphaned_tabs_threshold);
  syncer::EntityChangeList orphaned_tab_change_list;
  orphaned_tab_change_list.push_back(
      CreateEntityChange(orphaned_tab.ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_ADD));

  bridge_->ApplySyncChanges(bridge_->CreateMetadataChangeList(),
                            std::move(orphaned_tab_change_list));

  EXPECT_TRUE(saved_tab_group_model_.Contains(orphaned_guid));
  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 1u);

  SavedTabGroup* orphaned_group_from_model =
      saved_tab_group_model_.Get(orphaned_guid);
  EXPECT_EQ(orphaned_group_from_model->saved_tabs().size(), 1u);
  EXPECT_TRUE(
      orphaned_group_from_model->ContainsTab(orphaned_tab.saved_tab_guid()));
  EXPECT_TRUE(AreGroupSpecificsEqual(
      *missing_group.ToSpecifics(), *orphaned_group_from_model->ToSpecifics()));
  EXPECT_TRUE(AreTabSpecificsEqual(
      *orphaned_tab.ToSpecifics(),
      *orphaned_group_from_model->GetTab(orphaned_tab.saved_tab_guid())
           ->ToSpecifics()));
}

// Verify that when we add data into the sync bridge the SavedTabGroupModel
// will reflect those changes.
TEST_F(SavedTabGroupSyncBridgeTest, AddSyncData) {
  syncer::EntityChangeList empty_change_list;
  bridge_->MergeSyncData(bridge_->CreateMetadataChangeList(),
                         std::move(empty_change_list));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);
  group.SetPosition(0);

  bridge_->ApplySyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  // Ensure all data passed by the bridge is the same.
  ASSERT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  EXPECT_TRUE(AreGroupSpecificsEqual(*group.ToSpecifics(),
                                     *group_from_model->ToSpecifics()));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreTabSpecificsEqual(
        *tab.ToSpecifics(),
        *group_from_model->GetTab(tab.saved_tab_guid())->ToSpecifics()));
  }

  // Ensure a tab added to an existing group in the bridge is added into the
  // model correctly.
  SavedTabGroupTab additional_tab(GURL("https://maps.google.com"), u"Maps",
                                  group.saved_guid());

  // Orphaned tabs are tabs that do not have a respective group stored in the
  // model. As such, these tabs are kept in local storage but not the model.
  SavedTabGroupTab orphaned_tab(GURL("https://mail.google.com"), u"Mail",
                                base::GUID::GenerateRandomV4());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      CreateEntityChange(additional_tab.ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_ADD));
  entity_change_list.push_back(
      (CreateEntityChange(orphaned_tab.ToSpecifics(),
                          syncer::EntityChange::ChangeType::ACTION_ADD)));

  bridge_->ApplySyncChanges(bridge_->CreateMetadataChangeList(),
                            std::move(entity_change_list));

  ASSERT_TRUE(group_from_model->ContainsTab(additional_tab.saved_tab_guid()));
  EXPECT_EQ(group_from_model->saved_tabs().size(), 3u);
  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreTabSpecificsEqual(
        *tab.ToSpecifics(),
        *group_from_model->GetTab(tab.saved_tab_guid())->ToSpecifics()));
  }
}

// Verify that ACTION_UPDATE performs the same as ACTION_ADD initially and that
// the model reflects the updated group data after subsequent calls.
TEST_F(SavedTabGroupSyncBridgeTest, UpdateSyncData) {
  syncer::EntityChangeList empty_change_list;
  bridge_->MergeSyncData(bridge_->CreateMetadataChangeList(),
                         std::move(empty_change_list));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);
  group.SetPosition(0);

  bridge_->ApplySyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  group.SetTitle(u"A new title");
  group.SetColor(tab_groups::TabGroupColorId::kRed);
  group.saved_tabs()[0].SetURL(GURL("https://youtube.com"));

  bridge_->ApplySyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_UPDATE));

  EXPECT_TRUE(AreGroupSpecificsEqual(*group.ToSpecifics(),
                                     *group_from_model->ToSpecifics()));

  for (const SavedTabGroupTab& tab : group.saved_tabs()) {
    ASSERT_TRUE(group_from_model->ContainsTab(tab.saved_tab_guid()));
    EXPECT_TRUE(AreTabSpecificsEqual(
        *tab.ToSpecifics(),
        *group_from_model->GetTab(tab.saved_tab_guid())->ToSpecifics()));
  }
}

// Verify that the correct elements are removed when ACTION_DELETE is called.
TEST_F(SavedTabGroupSyncBridgeTest, DeleteSyncData) {
  syncer::EntityChangeList empty_change_list;
  bridge_->MergeSyncData(bridge_->CreateMetadataChangeList(),
                         std::move(empty_change_list));

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  EXPECT_EQ(group.saved_tabs().size(), 2u);

  bridge_->ApplySyncChanges(
      bridge_->CreateMetadataChangeList(),
      CreateEntityChangeListFromGroup(
          group, syncer::EntityChange::ChangeType::ACTION_ADD));

  ASSERT_TRUE(saved_tab_group_model_.Contains(group.saved_guid()));
  SavedTabGroup* group_from_model =
      saved_tab_group_model_.Get(group.saved_guid());

  // Ensure a deleted tab is deleted from the group correctly in the model.
  base::GUID tab_to_remove = group.saved_tabs()[0].saved_tab_guid();

  syncer::EntityChangeList delete_tab_change_list;
  delete_tab_change_list.push_back(
      CreateEntityChange(group.saved_tabs()[0].ToSpecifics(),
                         syncer::EntityChange::ChangeType::ACTION_DELETE));
  bridge_->ApplySyncChanges(bridge_->CreateMetadataChangeList(),
                            std::move(delete_tab_change_list));

  EXPECT_EQ(group_from_model->saved_tabs().size(), 1u);
  EXPECT_TRUE(
      AreTabSpecificsEqual(*group.saved_tabs()[1].ToSpecifics(),
                           *group_from_model->saved_tabs()[0].ToSpecifics()));
  EXPECT_FALSE(group_from_model->ContainsTab(tab_to_remove));

  // Ensure deleting a group deletes all the tabs in the group as well.
  syncer::EntityChangeList delete_group_change_list;

  delete_group_change_list.push_back(CreateEntityChange(
      group.ToSpecifics(), syncer::EntityChange::ChangeType::ACTION_DELETE));
  bridge_->ApplySyncChanges(bridge_->CreateMetadataChangeList(),
                            std::move(delete_group_change_list));

  EXPECT_EQ(saved_tab_group_model_.saved_tab_groups().size(), 0u);
  EXPECT_FALSE(saved_tab_group_model_.Contains(group.saved_guid()));
}

// Verify that locally added groups call add all group data to the processor.
TEST_F(SavedTabGroupSyncBridgeTest, AddGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  base::GUID group_guid = group.saved_guid();
  base::GUID tab_1_guid = tab_1.saved_tab_guid();
  base::GUID tab_2_guid = tab_2.saved_tab_guid();

  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _));

  saved_tab_group_model_.Add(std::move(group));
}

// Verify that locally removed groups remove all group data from the processor.
TEST_F(SavedTabGroupSyncBridgeTest, RemoveGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  base::GUID group_guid = group.saved_guid();
  base::GUID tab_1_guid = tab_1.saved_tab_guid();
  base::GUID tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Delete(tab_1_guid.AsLowercaseString(), _));
  EXPECT_CALL(processor_, Delete(tab_2_guid.AsLowercaseString(), _));
  EXPECT_CALL(processor_, Delete(group_guid.AsLowercaseString(), _));

  saved_tab_group_model_.Remove(group_guid);
}

// Verify that locally updated groups add all group data to the processor.
TEST_F(SavedTabGroupSyncBridgeTest, UpdateGroupLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  base::GUID group_guid = group.saved_guid();
  base::GUID tab_1_guid = tab_1.saved_tab_guid();
  base::GUID tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);

  tab_groups::TabGroupVisualData visual_data(
      u"New Title", tab_groups::TabGroupColorId::kYellow);
  saved_tab_group_model_.UpdateVisualData(group_guid, &visual_data);
}

// Verify that locally added tabs call put on the processor.
TEST_F(SavedTabGroupSyncBridgeTest, AddTabLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  SavedTabGroupTab tab_3(GURL("https://youtube.com"), u"Youtube",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  base::GUID group_guid = group.saved_guid();
  base::GUID tab_1_guid = tab_1.saved_tab_guid();
  base::GUID tab_2_guid = tab_2.saved_tab_guid();
  base::GUID tab_3_guid = tab_3.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Put(tab_3_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_1_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  // TODO(dljames): Because `tab_3` was added to the middle of the group, only
  // `tab_2` will have its position updated. Once tab ordering is implemented,
  // only the affected tabs will need to be updated. In that case, the Put()
  // call for tab_1 can be removed.
  saved_tab_group_model_.AddTabToGroup(group_guid, tab_3,
                                       /*update_tab_positions=*/true);
}

// Verify that locally removed tabs remove the correct tabs from the processor.
TEST_F(SavedTabGroupSyncBridgeTest, RemoveTabLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Goole",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  base::GUID group_guid = group.saved_guid();
  base::GUID tab_1_guid = tab_1.saved_tab_guid();
  base::GUID tab_2_guid = tab_2.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Delete(tab_1_guid.AsLowercaseString(), _));
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  saved_tab_group_model_.RemoveTabFromGroup(group_guid, tab_1_guid,
                                            /*update_tab_positions=*/true);
}

// Verify that locally updated tabs update the correct tabs in the processor.
TEST_F(SavedTabGroupSyncBridgeTest, UpdateTabLocally) {
  EXPECT_TRUE(saved_tab_group_model_.saved_tab_groups().empty());

  SavedTabGroup group(u"Test Title", tab_groups::TabGroupColorId::kBlue, {});
  SavedTabGroupTab tab_1(GURL("https://website.com"), u"Website Title",
                         group.saved_guid());
  SavedTabGroupTab tab_2(GURL("https://google.com"), u"Google",
                         group.saved_guid());
  SavedTabGroupTab tab_3(GURL("https://youtube.com"), u"Youtube",
                         group.saved_guid());
  group.AddTab(tab_1).AddTab(tab_2);

  base::GUID group_guid = group.saved_guid();
  base::GUID tab_1_guid = tab_1.saved_tab_guid();
  base::GUID tab_2_guid = tab_2.saved_tab_guid();
  base::GUID tab_3_guid = tab_3.saved_tab_guid();
  saved_tab_group_model_.Add(std::move(group));

  EXPECT_CALL(processor_, Delete(tab_1_guid.AsLowercaseString(), _));
  EXPECT_CALL(processor_, Put(tab_3_guid.AsLowercaseString(), _, _));
  EXPECT_CALL(processor_, Put(tab_2_guid.AsLowercaseString(), _, _)).Times(0);
  EXPECT_CALL(processor_, Put(group_guid.AsLowercaseString(), _, _)).Times(0);

  saved_tab_group_model_.ReplaceTabInGroupAt(group_guid, tab_1_guid, tab_3);
}
