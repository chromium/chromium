// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_bridge_mediator.h"

#include <map>
#include <memory>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/pref_names.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/saved_tab_groups/sync_data_type_configuration.h"
#include "components/saved_tab_groups/tab_group_sync_bridge_mediator.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

namespace {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

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
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavedTabGroupSpecificsToDataMigration, false);
    InitializeModelAndMediator();
  }

  ~TabGroupSyncBridgeMediatorTest() override = default;

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
              saved_tab_group_store_.get()));
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

TEST_F(TabGroupSyncBridgeMediatorTest, ShouldTransitionSavedTabGroupToShared) {
  ON_CALL(mock_saved_processor(), IsTrackingMetadata)
      .WillByDefault(Return(true));
  ON_CALL(mock_shared_processor(), IsTrackingMetadata)
      .WillByDefault(Return(true));

  SavedTabGroup group(u"group title", TabGroupColorId::kBlue, {}, 0);
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  SavedTabGroupTab tab(GURL("https://google.com"), u"tab title",
                       group.saved_guid(),
                       /*position=*/std::nullopt);
  group.AddTabLocally(tab);
  model().Add(group);
  ASSERT_TRUE(model().Get(group.saved_guid()));
  ASSERT_FALSE(model().Get(group.saved_guid())->is_shared_tab_group());

  // Both the tab and the group are expected to be added to the shared tab group
  // bridge, but only the group should be removed from the saved tab group
  // bridge.
  EXPECT_CALL(mock_saved_processor(), Delete);
  EXPECT_CALL(mock_shared_processor(),
              Put(group.saved_guid().AsLowercaseString(), _, _));
  EXPECT_CALL(mock_shared_processor(),
              Put(tab.saved_tab_guid().AsLowercaseString(), _, _));
  model().MakeTabGroupShared(group.local_group_id().value(), "collaboration");
}

}  // namespace

}  // namespace tab_groups
