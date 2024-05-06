// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/saved_tab_groups/empty_tab_group_store_delegate.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/saved_tab_groups/tab_group_store.h"
#include "components/saved_tab_groups/tab_group_store_id.h"
#include "components/saved_tab_groups/tab_group_sync_service_impl.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/test/fake_model_type_controller.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "components/sync/test/test_matchers.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;

namespace tab_groups {
namespace {

class MockTabGroupSyncServiceObserver : public TabGroupSyncService::Observer {
 public:
  MockTabGroupSyncServiceObserver() = default;
  ~MockTabGroupSyncServiceObserver() override = default;

  MOCK_METHOD(void, OnInitialized, ());
  MOCK_METHOD(void, OnTabGroupAdded, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupUpdated, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const LocalTabGroupID&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, TriggerSource));
};

class MockTabGroupStore : public TabGroupStore {
 public:
  MockTabGroupStore()
      : TabGroupStore(std::make_unique<EmptyTabGroupStoreDelegate>()) {}
  ~MockTabGroupStore() override = default;

  MOCK_METHOD(void, Initialize, (TabGroupStore::InitCallback));
  MOCK_METHOD((std::map<base::Uuid, TabGroupIDMetadata>),
              GetAllTabGroupIDMetadata,
              (),
              (const));
  MOCK_METHOD(std::optional<TabGroupIDMetadata>,
              GetTabGroupIDMetadata,
              (const base::Uuid&),
              (const));
  MOCK_METHOD(void,
              StoreTabGroupIDMetadata,
              (const base::Uuid&, const TabGroupIDMetadata&));
  MOCK_METHOD(void, DeleteTabGroupIDMetadata, (const base::Uuid&));
};

MATCHER_P(UuidEq, uuid, "") {
  return arg.saved_guid() == uuid;
}

}  // namespace

class TabGroupSyncServiceTest : public testing::Test {
 public:
  TabGroupSyncServiceTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        fake_controller_delegate_(syncer::SAVED_TAB_GROUP),
        group_1_(test::CreateTestSavedTabGroup()),
        group_2_(test::CreateTestSavedTabGroup()),
        group_3_(test::CreateTestSavedTabGroup()),
        local_group_id_1_(test::GenerateRandomTabGroupID()),
        local_tab_id_1_(test::GenerateRandomTabID()) {}

  ~TabGroupSyncServiceTest() override = default;

  void SetUp() override {
    auto model = std::make_unique<SavedTabGroupModel>();
    model_ = model.get();
    auto tab_group_store = std::make_unique<MockTabGroupStore>();
    tab_group_store_ = tab_group_store.get();

    tab_group_sync_service_ = std::make_unique<TabGroupSyncServiceImpl>(
        std::move(model),
        std::make_unique<TabGroupSyncServiceImpl::SyncDataTypeConfiguration>(
            processor_.CreateForwardingProcessor(),
            syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
                store_.get())),
        nullptr, std::move(tab_group_store));
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(processor_, GetControllerDelegate())
        .WillByDefault(testing::Return(fake_controller_delegate_.GetWeakPtr()));
    observer_ = std::make_unique<MockTabGroupSyncServiceObserver>();
    tab_group_sync_service_->AddObserver(observer_.get());
    task_environment_.RunUntilIdle();

    InitializeTestGroups();
  }

  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &processor_;
  }

  void TearDown() override {
    tab_group_sync_service_->RemoveObserver(observer_.get());
    model_ = nullptr;
  }

  void InitializeTestGroups() {
    base::Uuid id_1 = base::Uuid::GenerateRandomV4();
    base::Uuid id_2 = base::Uuid::GenerateRandomV4();
    base::Uuid id_3 = base::Uuid::GenerateRandomV4();

    const std::u16string title_1 = u"Group One";
    const std::u16string title_2 = u"Another Group";
    const std::u16string title_3 = u"The Three Musketeers";

    const tab_groups::TabGroupColorId& color_1 =
        tab_groups::TabGroupColorId::kGrey;
    const tab_groups::TabGroupColorId& color_2 =
        tab_groups::TabGroupColorId::kRed;
    const tab_groups::TabGroupColorId& color_3 =
        tab_groups::TabGroupColorId::kGreen;

    SavedTabGroupTab group_1_tab_1 = test::CreateSavedTabGroupTab(
        "A_Link", u"Only Tab", id_1, /*position=*/0);
    group_1_tab_1.SetLocalTabID(local_tab_id_1_);
    std::vector<SavedTabGroupTab> group_1_tabs = {group_1_tab_1};
    std::vector<SavedTabGroupTab> group_2_tabs = {
        test::CreateSavedTabGroupTab("One_Link", u"One Of Two", id_2,
                                     /*position=*/0),
        test::CreateSavedTabGroupTab("Two_Link", u"Second", id_2,
                                     /*position=*/1)};
    std::vector<SavedTabGroupTab> group_3_tabs = {
        test::CreateSavedTabGroupTab("Athos", u"All For One", id_3,
                                     /*position=*/0),
        test::CreateSavedTabGroupTab("Porthos", u"And", id_3, /*position=*/1),
        test::CreateSavedTabGroupTab("Aramis", u"One For All", id_3,
                                     /*position=*/2)};

    group_1_ = SavedTabGroup(title_1, color_1, group_1_tabs, std::nullopt, id_1,
                             local_group_id_1_);
    group_2_ =
        SavedTabGroup(title_2, color_2, group_2_tabs, std::nullopt, id_2);
    group_3_ =
        SavedTabGroup(title_3, color_3, group_3_tabs, std::nullopt, id_3);

    model_->Add(group_1_);
    model_->Add(group_2_);
    model_->Add(group_3_);
  }

  void SetupTabGroupStore(bool finish_init) {
    // Set up TabGroupStore with 3 IDs: 2 in both sync and store, 1 in sync but
    // not in store, and 1 in store but not in sync.
    std::map<base::Uuid, TabGroupIDMetadata> id_metadatas;

    TabGroupIDMetadata id_metadata_1(local_group_id_1_);
    id_metadatas.insert(std::make_pair(group_1_.saved_guid(), id_metadata_1));
    ON_CALL(*tab_group_store_, GetTabGroupIDMetadata(group_1_.saved_guid()))
        .WillByDefault(Return(id_metadata_1));

    TabGroupIDMetadata id_metadata_2(test::GenerateRandomTabGroupID());
    id_metadatas.insert(std::make_pair(group_2_.saved_guid(), id_metadata_2));
    ON_CALL(*tab_group_store_, GetTabGroupIDMetadata(group_2_.saved_guid()))
        .WillByDefault(Return(id_metadata_2));

    base::Uuid uuid_4 = base::Uuid::GenerateRandomV4();
    TabGroupIDMetadata id_metadata_4(test::GenerateRandomTabGroupID());
    id_metadatas.insert(std::make_pair(uuid_4, id_metadata_4));
    ON_CALL(*tab_group_store_, GetTabGroupIDMetadata(uuid_4))
        .WillByDefault(Return(id_metadata_4));

    ON_CALL(*tab_group_store_, GetAllTabGroupIDMetadata())
        .WillByDefault(Return(id_metadatas));

    // Finish store init if requested.
    ON_CALL(*tab_group_store_, Initialize(_))
        .WillByDefault(
            Invoke([finish_init](TabGroupStore::InitCallback callback) {
              if (finish_init) {
                std::move(callback).Run();
              }
            }));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<MockTabGroupSyncServiceObserver> observer_;
  std::unique_ptr<TabGroupSyncServiceImpl> tab_group_sync_service_;
  syncer::FakeModelTypeControllerDelegate fake_controller_delegate_;
  raw_ptr<MockTabGroupStore> tab_group_store_;

  SavedTabGroup group_1_;
  SavedTabGroup group_2_;
  SavedTabGroup group_3_;
  LocalTabGroupID local_group_id_1_;
  LocalTabID local_tab_id_1_;
};

TEST_F(TabGroupSyncServiceTest, ServiceConstruction) {
  EXPECT_TRUE(tab_group_sync_service_->GetSavedTabGroupControllerDelegate());
}

TEST_F(TabGroupSyncServiceTest, GetAllGroups) {
  auto all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_EQ(all_groups.size(), 3u);
  EXPECT_EQ(all_groups[0].saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(all_groups[1].saved_guid(), group_2_.saved_guid());
  EXPECT_EQ(all_groups[2].saved_guid(), group_3_.saved_guid());
}

TEST_F(TabGroupSyncServiceTest, GetGroup) {
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  EXPECT_EQ(group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(group->title(), group_1_.title());
  EXPECT_EQ(group->color(), group_1_.color());
  test::CompareSavedTabGroupTabs(group->saved_tabs(), group_1_.saved_tabs());
}

TEST_F(TabGroupSyncServiceTest, GetDeletedGroupIds) {
  // Setup TabGroupStore with 3 IDs: 2 in both sync and store, 1 in sync but not
  // in store, and 1 in store but not in sync.
  std::map<base::Uuid, TabGroupIDMetadata> id_metadatas;
  id_metadatas.insert(std::make_pair(group_1_.saved_guid(),
                                     TabGroupIDMetadata(local_group_id_1_)));
  id_metadatas.insert(
      std::make_pair(group_2_.saved_guid(),
                     TabGroupIDMetadata(test::GenerateRandomTabGroupID())));

  TabGroupIDMetadata id_metadata_4(test::GenerateRandomTabGroupID());
  id_metadatas.insert(
      std::make_pair(base::Uuid::GenerateRandomV4(), id_metadata_4));
  EXPECT_CALL(*tab_group_store_, GetAllTabGroupIDMetadata())
      .WillOnce(Return(id_metadatas));

  auto deleted_ids = tab_group_sync_service_->GetDeletedGroupIds();
  EXPECT_EQ(deleted_ids.size(), 1u);
  EXPECT_TRUE(base::Contains(deleted_ids, id_metadata_4.local_tab_group_id));
}

TEST_F(TabGroupSyncServiceTest, AddGroup) {
  // Add a new group.
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);

  TabGroupIDMetadata id_metadata(tab_group_id);
  EXPECT_CALL(
      *tab_group_store_,
      StoreTabGroupIDMetadata(Eq(group_4.saved_guid()), Eq(id_metadata)));

  tab_group_sync_service_->AddGroup(group_4);

  // Verify model internals.
  EXPECT_TRUE(model_->Contains(group_4.saved_guid()));
  EXPECT_EQ(model_->GetIndexOf(group_4.saved_guid()), 3);
  EXPECT_EQ(model_->Count(), 4);

  // Query the group via service and verify members.
  auto group = tab_group_sync_service_->GetGroup(group_4.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(group->saved_guid(), group_4.saved_guid());
  EXPECT_EQ(group->title(), group_4.title());
  EXPECT_EQ(group->color(), group_4.color());
  test::CompareSavedTabGroupTabs(group->saved_tabs(), group_4.saved_tabs());
}

TEST_F(TabGroupSyncServiceTest, RemoveGroupByLocalId) {
  // Add a group.
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group_4);
  EXPECT_TRUE(
      tab_group_sync_service_->GetGroup(group_4.saved_guid()).has_value());

  // Remove the group and verify.
  EXPECT_CALL(*tab_group_store_,
              DeleteTabGroupIDMetadata(Eq(group_4.saved_guid())));
  tab_group_sync_service_->RemoveGroup(tab_group_id);
  EXPECT_EQ(tab_group_sync_service_->GetGroup(group_4.saved_guid()),
            std::nullopt);

  // Verify model internals.
  EXPECT_FALSE(model_->Contains(group_4.saved_guid()));
  EXPECT_EQ(model_->Count(), 3);
}

TEST_F(TabGroupSyncServiceTest, RemoveGroupBySyncId) {
  // Remove the group and verify.
  EXPECT_CALL(*tab_group_store_,
              DeleteTabGroupIDMetadata(Eq(group_1_.saved_guid())));
  tab_group_sync_service_->RemoveGroup(group_1_.saved_guid());
  EXPECT_EQ(tab_group_sync_service_->GetGroup(group_1_.saved_guid()),
            std::nullopt);

  // Verify model internals.
  EXPECT_FALSE(model_->Contains(group_1_.saved_guid()));
  EXPECT_EQ(model_->Count(), 2);
}

TEST_F(TabGroupSyncServiceTest, UpdateVisualData) {
  tab_groups::TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  tab_group_sync_service_->UpdateVisualData(local_group_id_1_, &visual_data);

  auto group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(group.has_value());

  EXPECT_EQ(group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(group->title(), visual_data.title());
  EXPECT_EQ(group->color(), visual_data.color());
}

TEST_F(TabGroupSyncServiceTest, UpdateLocalTabGroupMapping) {
  LocalTabGroupID local_id_2 = test::GenerateRandomTabGroupID();
  TabGroupIDMetadata id_metadata(local_id_2);
  EXPECT_CALL(
      *tab_group_store_,
      StoreTabGroupIDMetadata(Eq(group_1_.saved_guid()), Eq(id_metadata)));
  tab_group_sync_service_->UpdateLocalTabGroupMapping(group_1_.saved_guid(),
                                                      local_id_2);

  auto retrieved_group = tab_group_sync_service_->GetGroup(local_id_2);
  EXPECT_TRUE(retrieved_group.has_value());

  EXPECT_EQ(retrieved_group->local_group_id().value(), local_id_2);
  EXPECT_EQ(retrieved_group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(retrieved_group->title(), group_1_.title());
  EXPECT_EQ(retrieved_group->color(), group_1_.color());

  test::CompareSavedTabGroupTabs(retrieved_group->saved_tabs(),
                                 group_1_.saved_tabs());
}

TEST_F(TabGroupSyncServiceTest, RemoveLocalTabGroupMapping) {
  auto retrieved_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_CALL(*tab_group_store_,
              DeleteTabGroupIDMetadata(Eq(group_1_.saved_guid())));
  tab_group_sync_service_->RemoveLocalTabGroupMapping(local_group_id_1_);

  retrieved_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_FALSE(retrieved_group.has_value());
}

TEST_F(TabGroupSyncServiceTest, AddTab) {
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());
}

TEST_F(TabGroupSyncServiceTest, AddUpdateRemoveTabWithUnknownGroupId) {
  auto unknown_group_id = test::GenerateRandomTabGroupID();
  auto local_tab_id = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(unknown_group_id, local_tab_id,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(unknown_group_id);
  EXPECT_FALSE(group.has_value());

  tab_group_sync_service_->UpdateTab(unknown_group_id, local_tab_id,
                                     u"random tab title",
                                     GURL("www.google.com"), std::nullopt);

  group = tab_group_sync_service_->GetGroup(unknown_group_id);
  EXPECT_FALSE(group.has_value());

  tab_group_sync_service_->RemoveTab(unknown_group_id, local_tab_id);
}

TEST_F(TabGroupSyncServiceTest, RemoveTab) {
  // Add a new tab.
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());

  // Remove tab.
  tab_group_sync_service_->RemoveTab(local_group_id_1_, local_tab_id_2);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(1u, group->saved_tabs().size());

  // Remove the last tab. The group should be removed from the model.
  EXPECT_CALL(*tab_group_store_,
              DeleteTabGroupIDMetadata(Eq(group_1_.saved_guid())));
  tab_group_sync_service_->RemoveTab(local_group_id_1_, local_tab_id_1_);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_FALSE(group.has_value());
}

TEST_F(TabGroupSyncServiceTest, UpdateTab) {
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  // Update tab.
  std::u16string new_title = u"tab title 2";
  GURL new_url = GURL("www.example.com");
  tab_group_sync_service_->UpdateTab(local_group_id_1_, local_tab_id_2,
                                     new_title, new_url, 2);
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());

  // Verify updated tab.
  auto* updated_tab = group->GetTab(local_tab_id_2);
  EXPECT_TRUE(updated_tab);
  EXPECT_EQ(new_title, updated_tab->title());
  EXPECT_EQ(new_url, updated_tab->url());
}

TEST_F(TabGroupSyncServiceTest, UpdateLocalTabId) {
  auto tab_guid = group_1_.saved_tabs()[0].saved_tab_guid();
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->UpdateLocalTabId(local_group_id_1_, tab_guid,
                                            local_tab_id_2);
  auto group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(1u, group->saved_tabs().size());

  // Verify updated tab.
  auto* updated_tab = group->GetTab(tab_guid);
  EXPECT_TRUE(updated_tab);
  EXPECT_EQ(local_tab_id_2, updated_tab->local_tab_id().value());
}

TEST_F(TabGroupSyncServiceTest, AddObserverBeforeInitialize) {
  SetupTabGroupStore(true);
  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  model_->LoadStoredEntries(std::vector<sync_pb::SavedTabGroupSpecifics>());
}

TEST_F(TabGroupSyncServiceTest, AddObserverAfterInitialize) {
  SetupTabGroupStore(true);
  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  model_->LoadStoredEntries(std::vector<sync_pb::SavedTabGroupSpecifics>());

  auto observer2 = std::make_unique<MockTabGroupSyncServiceObserver>();
  EXPECT_CALL(*observer2, OnInitialized()).Times(1);
  tab_group_sync_service_->AddObserver(observer2.get());
}

TEST_F(TabGroupSyncServiceTest, InitIsNotCompleteUntilMappingsAreRead) {
  SetupTabGroupStore(false);
  EXPECT_CALL(*observer_, OnInitialized()).Times(0);
  model_->LoadStoredEntries(std::vector<sync_pb::SavedTabGroupSpecifics>());
}

TEST_F(TabGroupSyncServiceTest, MappingsAreFixedOnStartup) {
  SetupTabGroupStore(true);
  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  model_->LoadStoredEntries(std::vector<sync_pb::SavedTabGroupSpecifics>());

  // Group 2 is an open group that has mapping persisted.
  auto group2 = tab_group_sync_service_->GetGroup(group_2_.saved_guid());
  EXPECT_TRUE(group2->local_group_id());

  // Group 3 is a closed group that doesn't have mapping entry.
  auto group3 = tab_group_sync_service_->GetGroup(group_3_.saved_guid());
  EXPECT_FALSE(group3->local_group_id());
}

TEST_F(TabGroupSyncServiceTest, MappingsAreNotFixedIfSetupNotComplete) {
  SetupTabGroupStore(false);
  EXPECT_CALL(*observer_, OnInitialized()).Times(0);
  model_->LoadStoredEntries(std::vector<sync_pb::SavedTabGroupSpecifics>());

  auto group = tab_group_sync_service_->GetGroup(group_2_.saved_guid());
  EXPECT_FALSE(group->local_group_id());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedFromRemoteSource) {
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->AddedFromSync(group_4);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedFromLocalSource) {
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->Add(group_4);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedFromRemoteSource) {
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->UpdatedVisualDataFromSync(group_1_.saved_guid(), &visual_data);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedFromLocalSource) {
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->UpdateVisualData(group_1_.saved_guid(), &visual_data);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedOnTabGroupIdMappingChange) {
  // Close a group.
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->OnGroupClosedInTabStrip(local_group_id_1_);

  // Open a group.
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_2_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->OnGroupOpenedInTabStrip(group_2_.saved_guid(),
                                  test::GenerateRandomTabGroupID());
}

TEST_F(TabGroupSyncServiceTest, TabIDMappingIsCleardOnGroupClose) {
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group->local_group_id().has_value());
  EXPECT_TRUE(group->saved_tabs()[0].local_tab_id().has_value());

  // Close a group.
  model_->OnGroupClosedInTabStrip(local_group_id_1_);

  // Verify that tab IDs are unmapped.
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_FALSE(group->local_group_id().has_value());
  EXPECT_FALSE(group->saved_tabs()[0].local_tab_id().has_value());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedNoTabs) {
  // Create a group with no tabs. Observers won't be notified.
  SavedTabGroup group_4 = test::CreateTestSavedTabGroupWithNoTabs();
  base::Uuid group_id = group_4.saved_guid();
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->AddedFromSync(group_4);

  // Update visuals. Observers still won't be notified.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  model_->UpdatedVisualDataFromSync(group_id, &visual_data);

  // Add a tab to the group. Observers will be notified as an Add event.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(1);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  SavedTabGroupTab tab =
      test::CreateSavedTabGroupTab("A_Link", u"Tab", group_id);
  model_->AddTabToGroupFromSync(group_id, tab);

  // Update visuals. Observers will be notified as an Update event.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->UpdatedVisualDataFromSync(group_id, &visual_data);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupRemovedFromRemoteSource) {
  // Removig group having local ID.
  EXPECT_CALL(*observer_,
              OnTabGroupRemoved(
                  testing::TypedEq<const LocalTabGroupID&>(local_group_id_1_),
                  Eq(TriggerSource::REMOTE)))
      .Times(1);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->RemovedFromSync(group_1_.saved_guid());

  // Remove a group with no local ID.
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_2_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->RemovedFromSync(group_2_.saved_guid());

  // Try removing a group that doesn't exist.
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->RemovedFromSync(group_1_.saved_guid());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupRemovedFromLocalSource) {
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->Remove(group_1_.local_group_id().value());
}

}  // namespace tab_groups
