// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {

using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::UnorderedElementsAre;

MATCHER_P3(HasSharedGroupMetadata, title, color, collaboration_id, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color &&
         arg.collaboration_id() == collaboration_id;
}

class MockTabGroupModelObserver : public SavedTabGroupModelObserver {
 public:
  explicit MockTabGroupModelObserver(SavedTabGroupModel* model) {
    observation_.Observe(model);
  }

  MOCK_METHOD(void, SavedTabGroupRemovedFromSync, (const SavedTabGroup*));
  MOCK_METHOD(void,
              SavedTabGroupUpdatedFromSync,
              (const base::Uuid&, const std::optional<base::Uuid>&));

 private:
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};
};

sync_pb::SharedTabGroupDataSpecifics MakeTabGroupSpecifics(
    const std::string& title,
    sync_pb::SharedTabGroup::Color color) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  sync_pb::SharedTabGroup* tab_group = specifics.mutable_tab_group();
  tab_group->set_title(title);
  tab_group->set_color(color);
  return specifics;
}

syncer::EntityData CreateEntityData(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_data() = specifics;
  entity_data.collaboration_id = collaboration_id;
  entity_data.name = specifics.guid();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateAddEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateAdd(
      storage_key, CreateEntityData(specifics, collaboration_id));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateUpdate(
      storage_key, CreateEntityData(specifics, collaboration_id));
}

class SharedTabGroupDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupDataSyncBridgeTest()
      : mock_model_observer_(&saved_tab_group_model_),
        store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void InitializeBridge() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        &saved_tab_group_model_,
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), &pref_service_);
    task_environment_.RunUntilIdle();
  }

  size_t GetNumEntriesInStore() {
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries;
    base::RunLoop run_loop;
    store_->ReadAllData(base::BindLambdaForTesting(
        [&run_loop, &entries](
            const std::optional<syncer::ModelError>& error,
            std::unique_ptr<syncer::ModelTypeStore::RecordList> data) {
          entries = std::move(data);
          run_loop.Quit();
        }));
    run_loop.Run();
    return entries->size();
  }

  SharedTabGroupDataSyncBridge* bridge() { return bridge_.get(); }
  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &processor_;
  }
  SavedTabGroupModel* model() { return &saved_tab_group_model_; }
  testing::NiceMock<MockTabGroupModelObserver>& mock_model_observer() {
    return mock_model_observer_;
  }

 private:
  // In memory model type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  SavedTabGroupModel saved_tab_group_model_;
  testing::NiceMock<MockTabGroupModelObserver> mock_model_observer_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SharedTabGroupDataSyncBridge> bridge_;
};

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnClientTag) {
  InitializeBridge();
  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()
                   ->GetClientTag(CreateEntityData(
                       MakeTabGroupSpecifics("test title",
                                             sync_pb::SharedTabGroup::GREEN),
                       "collaboration"))
                   .empty());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCallModelReadyToSync) {
  EXPECT_CALL(*mock_processor(), ModelReadyToSync).WillOnce(Invoke([]() {}));

  // This already invokes RunUntilIdle, so the call above is expected to happen.
  InitializeBridge();
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAddRemoteGroupsAtInitialSync) {
  InitializeBridge();

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteGroupsAtIncrementalUpdate) {
  InitializeBridge();

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldUpdateExistingGroup) {
  InitializeBridge();

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id1 = "collaboration";
  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id1));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);
  change_list.clear();

  group_specifics.mutable_tab_group()->set_title("updated title");
  group_specifics.mutable_tab_group()->set_color(sync_pb::SharedTabGroup::CYAN);
  change_list.push_back(
      CreateUpdateEntityChange(group_specifics, collaboration_id1));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("updated title",
                                 tab_groups::TabGroupColorId::kCyan,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldDeleteExistingGroup) {
  InitializeBridge();

  SavedTabGroup group_to_delete(u"title", tab_groups::TabGroupColorId::kBlue,
                                /*urls=*/{}, /*position=*/std::nullopt);
  group_to_delete.SetCollaborationId("collaboration");
  group_to_delete.AddTabLocally(SavedTabGroupTab(
      GURL("https://website.com"), u"Website Title",
      group_to_delete.saved_guid(), /*position=*/std::nullopt));
  model()->Add(group_to_delete);
  model()->Add(SavedTabGroup(u"title 2", tab_groups::TabGroupColorId::kGrey,
                             /*urls=*/{}, /*position=*/std::nullopt)
                   .SetCollaborationId("collaboration 2"));
  ASSERT_EQ(model()->Count(), 2);

  syncer::EntityChangeList change_list;
  change_list.push_back(syncer::EntityChange::CreateDelete(
      group_to_delete.saved_guid().AsLowercaseString()));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(HasSharedGroupMetadata(
          "title 2", tab_groups::TabGroupColorId::kGrey, "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCheckValidEntities) {
  InitializeBridge();

  EXPECT_TRUE(bridge()->IsEntityDataValid(CreateEntityData(
      MakeTabGroupSpecifics("test title", sync_pb::SharedTabGroup::GREEN),
      "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldRemoveLocalGroupsOnDisableSync) {
  InitializeBridge();

  // Initialize the model with some initial data. Create 2 entities to make it
  // sure that each of them is being deleted.
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::GREEN),
      "collaboration"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);
  ASSERT_EQ(GetNumEntriesInStore(), 2u);
  change_list.clear();

  // Stop sync and verify that data is removed from the model.
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(model()->Count(), 0);
  EXPECT_EQ(GetNumEntriesInStore(), 0u);
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldNotifyObserversOnDisableSync) {
  InitializeBridge();

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Observers must be notified for closed groups and tabs to make it sure that
  // both will be closed.
  EXPECT_CALL(mock_model_observer(), SavedTabGroupRemovedFromSync);
  EXPECT_CALL(mock_model_observer(),
              SavedTabGroupUpdatedFromSync(Eq(group.saved_guid()),
                                           Eq(tab1.saved_tab_guid())));
  // TODO(crbug.com/319521964): uncomment the following line once fixed.
  // EXPECT_CALL(mock_model_observer(),
  // SavedTabGroupUpdatedFromSync(Eq(group.saved_guid()),
  // Eq(tab2.saved_tab_guid())));
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
}

}  // namespace
}  // namespace tab_groups
