// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {

using testing::Invoke;
using testing::Return;
using testing::UnorderedElementsAre;

MATCHER_P2(HasGroupMetadata, title, color, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color;
}

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
    const sync_pb::SharedTabGroupDataSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_data() = specifics;
  entity_data.name = specifics.guid();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateAddEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateAdd(storage_key,
                                         CreateEntityData(specifics));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateUpdate(storage_key,
                                            CreateEntityData(specifics));
}

class SharedTabGroupDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupDataSyncBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void InitializeBridge() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        &saved_tab_group_model_,
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), &pref_service_);
    task_environment_.RunUntilIdle();
  }

  SharedTabGroupDataSyncBridge* bridge() { return bridge_.get(); }
  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &processor_;
  }
  SavedTabGroupModel* model() { return &saved_tab_group_model_; }

 private:
  // In memory model type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  SavedTabGroupModel saved_tab_group_model_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SharedTabGroupDataSyncBridge> bridge_;
};

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnClientTag) {
  InitializeBridge();
  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()
                   ->GetClientTag(CreateEntityData(MakeTabGroupSpecifics(
                       "test title", sync_pb::SharedTabGroup::GREEN)))
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
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE)));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED)));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasGroupMetadata("title", tab_groups::TabGroupColorId::kBlue),
          HasGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteGroupsAtIncrementalUpdate) {
  InitializeBridge();

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE)));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED)));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasGroupMetadata("title", tab_groups::TabGroupColorId::kBlue),
          HasGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldUpdateExistingGroup) {
  InitializeBridge();

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(group_specifics));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED)));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);
  change_list.clear();

  group_specifics.mutable_tab_group()->set_title("updated title");
  group_specifics.mutable_tab_group()->set_color(sync_pb::SharedTabGroup::CYAN);
  change_list.push_back(CreateUpdateEntityChange(group_specifics));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasGroupMetadata("updated title", tab_groups::TabGroupColorId::kCyan),
          HasGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldDeleteExistingGroup) {
  InitializeBridge();

  SavedTabGroup group_to_delete(u"title", tab_groups::TabGroupColorId::kBlue,
                                /*urls=*/{}, /*position=*/std::nullopt);
  group_to_delete.AddTabLocally(SavedTabGroupTab(
      GURL("https://website.com"), u"Website Title",
      group_to_delete.saved_guid(), /*position=*/std::nullopt));
  model()->Add(group_to_delete);
  model()->Add(SavedTabGroup(u"title 2", tab_groups::TabGroupColorId::kGrey,
                             /*urls=*/{}, /*position=*/std::nullopt));
  ASSERT_EQ(model()->Count(), 2);

  syncer::EntityChangeList change_list;
  change_list.push_back(syncer::EntityChange::CreateDelete(
      group_to_delete.saved_guid().AsLowercaseString()));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(model()->saved_tab_groups(),
              UnorderedElementsAre(HasGroupMetadata(
                  "title 2", tab_groups::TabGroupColorId::kGrey)));
}

}  // namespace
}  // namespace tab_groups
