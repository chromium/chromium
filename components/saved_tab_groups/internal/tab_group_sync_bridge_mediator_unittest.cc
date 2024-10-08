// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_bridge_mediator.h"

#include <map>
#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"
#include "components/saved_tab_groups/internal/tab_group_sync_bridge_mediator.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

namespace {

using tab_groups::test::HasSavedGroupMetadata;
using tab_groups::test::HasSharedGroupMetadata;
using tab_groups::test::HasTabMetadata;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Return;
using testing::UnorderedElementsAre;

constexpr char kCollaborationId[] = "collaboration";

class MockSavedTabGroupModelObserver : public SavedTabGroupModelObserver {
 public:
  explicit MockSavedTabGroupModelObserver(SavedTabGroupModel* model) {
    observation_.Observe(model);
  }

  MOCK_METHOD(void, SavedTabGroupModelLoaded, ());

 private:
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};
};

class TabGroupSyncBridgeMediatorTest : public testing::Test {
 public:
  TabGroupSyncBridgeMediatorTest()
      : saved_tab_group_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        shared_tab_group_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavedTabGroupSpecificsToDataMigration, false);
    InitializeModelAndMediator();
  }

  ~TabGroupSyncBridgeMediatorTest() override = default;

  // Simulates storing sync metadata with collaboration IDs. The metadata is
  // used on loading data from the disk for the shared tab group sync bridge.
  void StoreCollaborationIdsToMetadata() {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
        shared_tab_group_store_->CreateWriteBatch();
    syncer::MetadataChangeList* metadata_change_list =
        write_batch->GetMetadataChangeList();
    for (const SavedTabGroup* group : model().GetSharedTabGroupsOnly()) {
      CHECK(group->collaboration_id().has_value());
      const std::string group_storage_key =
          group->saved_guid().AsLowercaseString();

      // Use the same metadata for all the tabs and the group itself.
      sync_pb::EntityMetadata metadata;
      metadata.mutable_collaboration()->set_collaboration_id(
          group->collaboration_id().value());
      metadata_change_list->UpdateMetadata(group_storage_key, metadata);
      for (const SavedTabGroupTab& tab : group->saved_tabs()) {
        const std::string tab_storage_key =
            tab.saved_tab_guid().AsLowercaseString();
        metadata_change_list->UpdateMetadata(tab_storage_key, metadata);
      }
    }
    base::RunLoop run_loop;
    shared_tab_group_store_->CommitWriteBatch(
        std::move(write_batch),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               const std::optional<syncer::ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
  }

  // Simulate browser restart and re-initialize the model and sync bridges.
  void InitializeModelAndMediator(bool initialize_shared_tab_group = true) {
    Reset();
    model_ = std::make_unique<SavedTabGroupModel>();

    auto saved_sync_configuration = std::make_unique<SyncDataTypeConfiguration>(
        mock_saved_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
            saved_tab_group_store_.get()));

    std::unique_ptr<SyncDataTypeConfiguration> shared_sync_configuration;
    if (initialize_shared_tab_group) {
      shared_sync_configuration = std::make_unique<SyncDataTypeConfiguration>(
          mock_shared_processor_.CreateForwardingProcessor(),
          syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
              shared_tab_group_store_.get()));
    }

    testing::NiceMock<MockSavedTabGroupModelObserver> model_observer(
        model_.get());
    base::RunLoop run_loop;
    EXPECT_CALL(model_observer, SavedTabGroupModelLoaded)
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }))
        .RetiresOnSaturation();
    bridge_mediator_ = std::make_unique<TabGroupSyncBridgeMediator>(
        model_.get(), &pref_service_, std::move(saved_sync_configuration),
        std::move(shared_sync_configuration));
    run_loop.Run();
  }

  SavedTabGroupModel& model() { return *model_; }
  TabGroupSyncBridgeMediator& bridge_mediator() { return *bridge_mediator_; }

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_saved_processor() {
    return mock_saved_processor_;
  }

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_shared_processor() {
    return mock_shared_processor_;
  }

 private:
  // Simulate browser shutdown and reset the bridges and the model.
  void Reset() {
    // Store sync metadata before cleaning up the model.
    if (model_) {
      StoreCollaborationIdsToMetadata();
    }
    // Bridges contain a pointer to the `model_` and must be cleaned up first.
    bridge_mediator_.reset();
    model_.reset();
  }

  base::test::TaskEnvironment task_environment_;

  TestingPrefServiceSimple pref_service_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>
      mock_saved_processor_;
  std::unique_ptr<syncer::DataTypeStore> saved_tab_group_store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>
      mock_shared_processor_;
  std::unique_ptr<syncer::DataTypeStore> shared_tab_group_store_;

  // Store in unique_ptr to be able to re-create simulating browser restart.
  std::unique_ptr<SavedTabGroupModel> model_;
  std::unique_ptr<TabGroupSyncBridgeMediator> bridge_mediator_;
};

TEST_F(TabGroupSyncBridgeMediatorTest, ShouldInitializeEmptySavedTabGroups) {
  // The model must be loaded because the bridge was initialized.
  EXPECT_TRUE(model().is_loaded());

  // The same but with disabled shared tab group data.
  InitializeModelAndMediator(/*initialize_shared_tab_group=*/false);
  EXPECT_TRUE(model().is_loaded());
}

TEST_F(TabGroupSyncBridgeMediatorTest, ShouldInitializeModelAfterRestart) {
  // The model must be loaded because the bridge was initialized.
  ASSERT_TRUE(model().is_loaded());

  SavedTabGroup group(u"group title", tab_groups::TabGroupColorId::kBlue, {},
                      0);
  group.AddTabLocally(SavedTabGroupTab(GURL("https://google.com"), u"tab title",
                                       group.saved_guid(),
                                       /*position=*/std::nullopt));
  model().Add(std::move(group));

  InitializeModelAndMediator();
  EXPECT_TRUE(model().is_loaded());
  EXPECT_EQ(model().Count(), 1);
}

TEST_F(TabGroupSyncBridgeMediatorTest, ShouldReturnSavedBridgeSyncing) {
  EXPECT_CALL(mock_saved_processor(), IsTrackingMetadata)
      .WillOnce(Return(true));
  EXPECT_TRUE(bridge_mediator().IsSavedBridgeSyncing());
}

TEST_F(TabGroupSyncBridgeMediatorTest, ShouldReturnSavedBridgeCacheGuid) {
  EXPECT_CALL(mock_saved_processor(), IsTrackingMetadata)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_saved_processor(), TrackedCacheGuid)
      .WillOnce(Return("cache_guid"));
  EXPECT_EQ(bridge_mediator().GetLocalCacheGuidForSavedBridge(), "cache_guid");
}

TEST_F(TabGroupSyncBridgeMediatorTest, ShouldReturnSavedBridgeNotSyncing) {
  EXPECT_CALL(mock_saved_processor(), IsTrackingMetadata)
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(bridge_mediator().IsSavedBridgeSyncing());
  EXPECT_EQ(bridge_mediator().GetLocalCacheGuidForSavedBridge(), std::nullopt);
}

TEST_F(TabGroupSyncBridgeMediatorTest, ShouldResolveDuplicatesOnLoad) {
  // The model is not expected to contain duplicate GUIDs, hence data
  // preparation is done in several steps:
  // 1. Initialize shared tab groups.
  // 2. Restart with saved tab groups only (shared tabs are not loaded from the
  //    store), and initialize duplicate saved tab groups.
  // 3. Restart with both types of groups.
  SavedTabGroup shared_group_1(u"shared group 1",
                               tab_groups::TabGroupColorId::kBlue, /*urls=*/{},
                               /*position=*/std::nullopt);
  shared_group_1.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab shared_tab_1(GURL("http://google.com/1"), u"shared tab 1",
                                shared_group_1.saved_guid(),
                                /*position=*/std::nullopt);
  SavedTabGroupTab shared_tab_2(GURL("http://google.com/2"), u"shared tab 2",
                                shared_group_1.saved_guid(),
                                /*position=*/std::nullopt);
  shared_group_1.AddTabLocally(shared_tab_1);
  shared_group_1.AddTabLocally(shared_tab_2);
  model().Add(shared_group_1);
  SavedTabGroup shared_group_2(u"shared group 2",
                               tab_groups::TabGroupColorId::kBlue, /*urls=*/{},
                               /*position=*/std::nullopt);
  shared_group_2.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab shared_tab_3(GURL("http://google.com/3"), u"shared tab 3",
                                shared_group_2.saved_guid(),
                                /*position=*/std::nullopt);
  shared_group_2.AddTabLocally(shared_tab_3);
  model().Add(shared_group_2);

  // Restart with only saved tab groups enabled to create duplicates.
  InitializeModelAndMediator(/*initialize_shared_tab_group=*/false);
  ASSERT_THAT(model().saved_tab_groups(), IsEmpty());

  // Add the whole duplicate of the shared tab group 1.
  SavedTabGroup group_1_saved_copy(
      u"group 1 saved copy", tab_groups::TabGroupColorId::kGreen, /*urls=*/{},
      /*position=*/std::nullopt, shared_group_1.saved_guid());
  group_1_saved_copy.AddTabLocally(SavedTabGroupTab(
      GURL("http://google.com/1"), u"saved tab 1",
      group_1_saved_copy.saved_guid(),
      /*position=*/std::nullopt, shared_tab_1.saved_tab_guid()));
  group_1_saved_copy.AddTabLocally(SavedTabGroupTab(
      GURL("http://google.com/2"), u"saved tab 2",
      group_1_saved_copy.saved_guid(),
      /*position=*/std::nullopt, shared_tab_2.saved_tab_guid()));
  model().Add(group_1_saved_copy);

  // Add duplicate tab to a new saved tab group.
  SavedTabGroup saved_group(u"saved group", tab_groups::TabGroupColorId::kGreen,
                            /*urls=*/{}, /*position=*/std::nullopt);
  SavedTabGroupTab tab_3_saved_copy(
      GURL("http://google.com/3"), u"saved tab 3", saved_group.saved_guid(),
      /*position=*/std::nullopt, shared_tab_3.saved_tab_guid());
  saved_group.AddTabLocally(tab_3_saved_copy);
  saved_group.AddTabLocally(SavedTabGroupTab(
      GURL("http://google.com/4"), u"saved tab 4", saved_group.saved_guid(),
      /*position=*/std::nullopt));
  model().Add(saved_group);

  // Restart the bridge with both types enabled, and verify the result.
  InitializeModelAndMediator(/*initialize_shared_tab_group=*/true);
  ASSERT_THAT(model().saved_tab_groups(),
              UnorderedElementsAre(
                  HasSharedGroupMetadata("shared group 1",
                                         tab_groups::TabGroupColorId::kBlue,
                                         kCollaborationId),
                  HasSharedGroupMetadata("shared group 2",
                                         tab_groups::TabGroupColorId::kBlue,
                                         kCollaborationId),
                  HasSavedGroupMetadata("saved group",
                                        tab_groups::TabGroupColorId::kGreen)));

  EXPECT_THAT(model().Get(shared_group_1.saved_guid())->saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("shared tab 1", "http://google.com/1"),
                  HasTabMetadata("shared tab 2", "http://google.com/2")));
  EXPECT_THAT(model().Get(shared_group_2.saved_guid())->saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("shared tab 3", "http://google.com/3")));
  // Saved tab 3 is a copy of the shared tab 3 above, hence it's not added to
  // the saved tab group.
  EXPECT_THAT(model().Get(saved_group.saved_guid())->saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("saved tab 4", "http://google.com/4")));
}

}  // namespace

}  // namespace tab_groups
