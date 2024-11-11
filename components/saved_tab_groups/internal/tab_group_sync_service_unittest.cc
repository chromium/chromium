// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"
#include "components/saved_tab_groups/internal/tab_group_sync_coordinator.h"
#include "components/saved_tab_groups/internal/tab_group_sync_metrics_logger_impl.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::An;
using testing::ByRef;
using testing::ContainerEq;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Matcher;
using testing::NotNull;
using testing::Return;
using testing::Sequence;
using testing::SizeIs;
using testing::WithArgs;

namespace tab_groups {
namespace {

const char kTestCacheGuid[] = "test_cache_guid";

class MockTabGroupSyncServiceObserver : public TabGroupSyncService::Observer {
 public:
  MockTabGroupSyncServiceObserver() = default;
  ~MockTabGroupSyncServiceObserver() override = default;

  MOCK_METHOD(void, OnInitialized, ());
  MOCK_METHOD(void, OnTabGroupAdded, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupUpdated, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const LocalTabGroupID&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, TriggerSource));
  MOCK_METHOD(void,
              OnTabGroupLocalIdChanged,
              (const base::Uuid&, const std::optional<LocalTabGroupID>&));
  MOCK_METHOD(void, OnTabGroupsReordered, (TriggerSource));
};

class MockTabGroupSyncCoordinator : public TabGroupSyncCoordinator {
 public:
  MockTabGroupSyncCoordinator() = default;
  ~MockTabGroupSyncCoordinator() override = default;

  MOCK_METHOD(void,
              HandleOpenTabGroupRequest,
              (const base::Uuid&, std::unique_ptr<TabGroupActionContext>));
  MOCK_METHOD(void,
              ConnectLocalTabGroup,
              (const base::Uuid&, const LocalTabGroupID&));
  MOCK_METHOD(void, DisconnectLocalTabGroup, (const LocalTabGroupID&));
  MOCK_METHOD(std::unique_ptr<ScopedLocalObservationPauser>,
              CreateScopedLocalObserverPauser,
              ());
  MOCK_METHOD(void, OnInitialized, ());
  MOCK_METHOD(void, OnTabGroupAdded, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupUpdated, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const LocalTabGroupID&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, TriggerSource));
};

MATCHER_P(UuidEq, uuid, "") {
  return arg.saved_guid() == uuid;
}

}  // namespace

class TabGroupSyncServiceTest : public testing::Test {
 public:
  TabGroupSyncServiceTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        saved_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        shared_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        decider_(std::make_unique<
                 optimization_guide::MockOptimizationGuideDecider>()),
        fake_controller_delegate_(syncer::SAVED_TAB_GROUP),
        group_1_(test::CreateTestSavedTabGroup()),
        group_2_(test::CreateTestSavedTabGroup()),
        group_3_(test::CreateTestSavedTabGroup()),
        group_4_(test::CreateTestSavedTabGroup()),
        local_group_id_1_(test::GenerateRandomTabGroupID()),
        local_tab_id_1_(test::GenerateRandomTabID()) {}

  ~TabGroupSyncServiceTest() override = default;

  void SetUp() override {
    auto model = std::make_unique<SavedTabGroupModel>();
    model_ = model.get();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavedTabGroupSpecificsToDataMigration, false);
    pref_service_.registry()->RegisterDictionaryPref(prefs::kDeletedTabGroupIds,
                                                     base::Value::Dict());
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kLocallyClosedRemoteTabGroupIds, base::Value::Dict());

    auto metrics_logger =
        std::make_unique<TabGroupSyncMetricsLoggerImpl>(&device_info_tracker_);

    EXPECT_CALL(*decider_, RegisterOptimizationTypes(_)).Times(1);
    tab_group_sync_service_ = std::make_unique<TabGroupSyncServiceImpl>(
        std::move(model),
        std::make_unique<SyncDataTypeConfiguration>(
            saved_processor_.CreateForwardingProcessor(),
            syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
                saved_store_.get())),
        std::make_unique<SyncDataTypeConfiguration>(
            shared_processor_.CreateForwardingProcessor(),
            syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
                shared_store_.get())),
        &pref_service_, std::move(metrics_logger), decider_.get(),
        identity_test_environment_.identity_manager());
    ON_CALL(saved_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(saved_processor_, TrackedCacheGuid())
        .WillByDefault(testing::Return(kTestCacheGuid));
    ON_CALL(saved_processor_, GetControllerDelegate())
        .WillByDefault(testing::Return(fake_controller_delegate_.GetWeakPtr()));
    ON_CALL(shared_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));

    auto coordinator =
        std::make_unique<testing::NiceMock<MockTabGroupSyncCoordinator>>();
    coordinator_ = coordinator.get();
    tab_group_sync_service_->SetCoordinator(std::move(coordinator));

    observer_ =
        std::make_unique<testing::NiceMock<MockTabGroupSyncServiceObserver>>();
    tab_group_sync_service_->AddObserver(observer_.get());
    task_environment_.RunUntilIdle();

    MaybeInitializeTestGroups();
    task_environment_.RunUntilIdle();
  }

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>*
  mock_processor() {
    return &saved_processor_;
  }

  void TearDown() override {
    tab_group_sync_service_->RemoveObserver(observer_.get());
    model_ = nullptr;
    coordinator_ = nullptr;
  }

  // Enable sub-classes to not load initial test groups.
  virtual void MaybeInitializeTestGroups() { InitializeTestGroups(); }

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

    group_1_ = SavedTabGroup(title_1, color_1, group_1_tabs, 0, id_1,
                             local_group_id_1_);
    group_2_ = SavedTabGroup(title_2, color_2, group_2_tabs, 1, id_2);
    group_3_ = SavedTabGroup(title_3, color_3, group_3_tabs, 2, id_3);

    model_->AddedLocally(group_1_);
    model_->AddedLocally(group_2_);
    model_->AddedLocally(group_3_);
    model_->UpdateLocalCacheGuid(/*old_cache_guid=*/std::nullopt,
                                 kTestCacheGuid);
  }

  void VerifyCacheGuids(const SavedTabGroup& group,
                        const SavedTabGroupTab* tab,
                        std::optional<std::string> group_creator_cache_guid,
                        std::optional<std::string> group_updater_cache_guid,
                        std::optional<std::string> tab_creator_cache_guid,
                        std::optional<std::string> tab_updater_cache_guid) {
    EXPECT_EQ(group_creator_cache_guid, group.creator_cache_guid());
    EXPECT_EQ(group_updater_cache_guid, group.last_updater_cache_guid());
    if (!tab) {
      return;
    }

    EXPECT_EQ(tab_creator_cache_guid, tab->creator_cache_guid());
    EXPECT_EQ(tab_updater_cache_guid, tab->last_updater_cache_guid());
  }

  void WaitForPostedTasks() {
    // Post a dummy task in the current thread and wait for its completion so
    // that any already posted tasks are completed.
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  signin::IdentityTestEnvironment identity_test_environment_;
  TestingPrefServiceSimple pref_service_;
  raw_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> saved_processor_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> shared_processor_;
  std::unique_ptr<syncer::DataTypeStore> saved_store_;
  std::unique_ptr<syncer::DataTypeStore> shared_store_;
  std::unique_ptr<testing::NiceMock<MockTabGroupSyncServiceObserver>> observer_;
  syncer::FakeDeviceInfoTracker device_info_tracker_;
  raw_ptr<testing::NiceMock<MockTabGroupSyncCoordinator>> coordinator_;
  std::unique_ptr<optimization_guide::MockOptimizationGuideDecider> decider_;
  std::unique_ptr<TabGroupSyncServiceImpl> tab_group_sync_service_;
  syncer::FakeDataTypeControllerDelegate fake_controller_delegate_;

  SavedTabGroup group_1_;
  SavedTabGroup group_2_;
  SavedTabGroup group_3_;
  SavedTabGroup group_4_;
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

  SavedTabGroup group_4(test::CreateTestSavedTabGroupWithNoTabs());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group_4);

  EXPECT_EQ(model_->Count(), 4);
  all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_EQ(all_groups.size(), 3u);
}

TEST_F(TabGroupSyncServiceTest, GetGroup) {
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  EXPECT_EQ(group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(group->title(), group_1_.title());
  EXPECT_EQ(group->color(), group_1_.color());
  test::CompareSavedTabGroupTabs(group->saved_tabs(), group_1_.saved_tabs());
}

TEST_F(TabGroupSyncServiceTest, GetDeletedGroupIdsUsingPrefs) {
  // Delete a group from sync. It should add the deleted ID to the pref.
  model_->RemovedFromSync(group_1_.saved_guid());
  WaitForPostedTasks();

  auto deleted_ids = tab_group_sync_service_->GetDeletedGroupIds();
  EXPECT_EQ(1u, deleted_ids.size());
  EXPECT_TRUE(base::Contains(deleted_ids, local_group_id_1_));

  // Now close out the group from tab model and notify service.
  // The entry should be cleaned up from prefs.
  tab_group_sync_service_->RemoveLocalTabGroupMapping(local_group_id_1_,
                                                      ClosingSource::kUnknown);

  deleted_ids = tab_group_sync_service_->GetDeletedGroupIds();
  EXPECT_EQ(0u, deleted_ids.size());
}

TEST_F(TabGroupSyncServiceTest,
       GetDeletedGroupIdsUsingPrefsWhileRemovedFromLocal) {
  // Delete a group from local. It should not add the entry to the prefs.
  model_->RemovedLocally(group_1_.saved_guid());
  WaitForPostedTasks();

  auto deleted_ids = tab_group_sync_service_->GetDeletedGroupIds();
  EXPECT_EQ(0u, deleted_ids.size());
}

TEST_F(TabGroupSyncServiceTest, AddGroup) {
  base::HistogramTester histogram_tester;
  // Add a new group.
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);

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
  EXPECT_FALSE(group->created_before_syncing_tab_groups());
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, std::nullopt, std::nullopt,
                   std::nullopt);

  test::CompareSavedTabGroupTabs(group->saved_tabs(), group_4.saved_tabs());
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.Created.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, AddGroup_BeforeInit) {
  // Add a new group.
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);

  EXPECT_FALSE(model_->Contains(group_4.saved_guid()));
  EXPECT_EQ(model_->Count(), 3);

  tab_group_sync_service_->SetIsInitializedForTesting(false);
  tab_group_sync_service_->AddGroup(group_4);
  EXPECT_FALSE(model_->Contains(group_4.saved_guid()));

  // Initialize model and add group 4.
  model_->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});
  WaitForPostedTasks();

  // Verify model internals.
  EXPECT_TRUE(model_->Contains(group_4.saved_guid()));
  EXPECT_EQ(model_->Count(), 4);
}

TEST_F(TabGroupSyncServiceTest, AddGroupWhenSignedOut) {
  // Add a new group while signed out.
  ON_CALL(saved_processor_, IsTrackingMetadata())
      .WillByDefault(testing::Return(false));

  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);

  tab_group_sync_service_->AddGroup(group_4);

  // Query the group via service and verify members.
  auto group = tab_group_sync_service_->GetGroup(group_4.saved_guid());
  EXPECT_EQ(group->saved_guid(), group_4.saved_guid());
  EXPECT_TRUE(group->created_before_syncing_tab_groups());
}

TEST_F(TabGroupSyncServiceTest, RemoveGroupByLocalId) {
  base::HistogramTester histogram_tester;
  // Add a group.
  SavedTabGroup group_4(test::CreateTestSavedTabGroup());
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group_4);
  EXPECT_TRUE(
      tab_group_sync_service_->GetGroup(group_4.saved_guid()).has_value());

  // Remove the group and verify.
  tab_group_sync_service_->RemoveGroup(tab_group_id);
  EXPECT_EQ(tab_group_sync_service_->GetGroup(group_4.saved_guid()),
            std::nullopt);

  // Verify model internals.
  EXPECT_FALSE(model_->Contains(group_4.saved_guid()));
  EXPECT_EQ(model_->Count(), 3);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.Removed.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, RemoveGroupBySyncId) {
  // Remove the group and verify.
  tab_group_sync_service_->RemoveGroup(group_1_.saved_guid());
  EXPECT_EQ(tab_group_sync_service_->GetGroup(group_1_.saved_guid()),
            std::nullopt);

  // Verify model internals.
  EXPECT_FALSE(model_->Contains(group_1_.saved_guid()));
  EXPECT_EQ(model_->Count(), 2);
}

TEST_F(TabGroupSyncServiceTest, UpdateVisualData) {
  base::HistogramTester histogram_tester;
  tab_groups::TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  tab_group_sync_service_->UpdateVisualData(local_group_id_1_, &visual_data);

  auto group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(group.has_value());

  EXPECT_EQ(group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(group->title(), visual_data.title());
  EXPECT_EQ(group->color(), visual_data.color());
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt, std::nullopt);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.VisualsChanged.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, OpenTabGroup) {
  EXPECT_CALL(*coordinator_,
              HandleOpenTabGroupRequest(group_2_.saved_guid(), testing::_))
      .Times(1);
  tab_group_sync_service_->OpenTabGroup(
      group_2_.saved_guid(), std::make_unique<TabGroupActionContext>());
}

TEST_F(TabGroupSyncServiceTest, ConnectLocalTabGroup) {
  LocalTabGroupID local_id = test::GenerateRandomTabGroupID();
  EXPECT_CALL(*coordinator_,
              ConnectLocalTabGroup(group_2_.saved_guid(), local_id))
      .Times(1);
  tab_group_sync_service_->ConnectLocalTabGroup(
      group_2_.saved_guid(), local_id, OpeningSource::kOpenedFromRevisitUi);
}

TEST_F(TabGroupSyncServiceTest, ConnectLocalTabGroup_BeforeInit) {
  LocalTabGroupID local_id = test::GenerateRandomTabGroupID();
  tab_group_sync_service_->SetIsInitializedForTesting(false);

  // Expect ConnectLocalTabGroup to not be called before init.
  EXPECT_CALL(*coordinator_, ConnectLocalTabGroup(_, _)).Times(0);

  tab_group_sync_service_->ConnectLocalTabGroup(
      group_2_.saved_guid(), local_id, OpeningSource::kAutoOpenedFromSync);
  // Initialize model and connect the group.
  EXPECT_CALL(*coordinator_,
              ConnectLocalTabGroup(group_2_.saved_guid(), local_id))
      .Times(1);
  model_->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, UpdateLocalTabGroupMapping_BeforeInit) {
  tab_group_sync_service_->SetIsInitializedForTesting(false);
  LocalTabGroupID local_id_4 = test::GenerateRandomTabGroupID();
  ASSERT_FALSE(group_4_.local_group_id().has_value());

  tab_group_sync_service_->UpdateLocalTabGroupMapping(
      group_4_.saved_guid(), local_id_4, OpeningSource::kUnknown);

  auto retrieved_group =
      tab_group_sync_service_->GetGroup(group_4_.saved_guid());
  EXPECT_FALSE(retrieved_group.has_value());

  // Initialize model and add group 4.
  model_->LoadStoredEntries(/*groups=*/{group_4_}, /*tabs=*/{});
  WaitForPostedTasks();

  retrieved_group = tab_group_sync_service_->GetGroup(group_4_.saved_guid());
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(retrieved_group->local_group_id().value(), local_id_4);
  EXPECT_EQ(retrieved_group->saved_guid(), group_4_.saved_guid());

  test::CompareSavedTabGroupTabs(retrieved_group->saved_tabs(),
                                 group_4_.saved_tabs());
}

TEST_F(TabGroupSyncServiceTest, UpdateLocalTabGroupMapping_AfterInit) {
  LocalTabGroupID local_id_2 = test::GenerateRandomTabGroupID();
  tab_group_sync_service_->UpdateLocalTabGroupMapping(
      group_1_.saved_guid(), local_id_2, OpeningSource::kUnknown);

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
  tab_group_sync_service_->RemoveLocalTabGroupMapping(local_group_id_1_,
                                                      ClosingSource::kUnknown);

  retrieved_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_FALSE(retrieved_group.has_value());
  // TODO
}

TEST_F(TabGroupSyncServiceTest, AddTab) {
  base::HistogramTester histogram_tester;
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  auto local_tab_id_2 = test::GenerateRandomTabID();
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, std::nullopt, std::nullopt,
                   std::nullopt);

  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabAdded.GroupCreateOrigin", 1u);

  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt, std::nullopt);
}

// Tests that addubg a tab to a shared group.
TEST_F(TabGroupSyncServiceTest, AddTabToSharedGroup) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_EQ(group->saved_tabs().size(), 1u);
  ASSERT_FALSE(group->saved_tabs()[0].is_pending_sanitization());
  tab_group_sync_service_->MakeTabGroupShared(local_group_id_1_,
                                              "collaboration");

  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();

  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_EQ(shared_group->saved_tabs().size(), 1u);

  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2, u"foo",
                                  GURL("https://www.google.com"), std::nullopt);
  auto local_tab_id_3 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_3, u"foo2",
                                  GURL("www.google.com"), std::nullopt);

  shared_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_EQ(shared_group->saved_tabs().size(), 3u);
  // Only tab 2 has pending sanitization as it is a HTTPS url.
  ASSERT_FALSE(shared_group->saved_tabs()[0].is_pending_sanitization());
  ASSERT_TRUE(shared_group->saved_tabs()[1].is_pending_sanitization());
  ASSERT_FALSE(shared_group->saved_tabs()[2].is_pending_sanitization());
}

TEST_F(TabGroupSyncServiceTest, AddUpdateRemoveTabWithUnknownGroupId) {
  base::HistogramTester histogram_tester;
  auto unknown_group_id = test::GenerateRandomTabGroupID();
  auto local_tab_id = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(unknown_group_id, local_tab_id,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(unknown_group_id);
  EXPECT_FALSE(group.has_value());

  const std::u16string title = u"random tab title";
  GURL url = GURL("https://www.google.com");
  tab_group_sync_service_->NavigateTab(unknown_group_id, local_tab_id, url,
                                       title);

  group = tab_group_sync_service_->GetGroup(unknown_group_id);
  EXPECT_FALSE(group.has_value());

  tab_group_sync_service_->RemoveTab(unknown_group_id, local_tab_id);

  // No histograms should be recorded.
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabAdded.GroupCreateOrigin", 0u);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabRemoved.GroupCreateOrigin", 0u);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin", 0u);
}

TEST_F(TabGroupSyncServiceTest, RemoveTab) {
  base::HistogramTester histogram_tester;
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
  VerifyCacheGuids(*group, nullptr, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt, std::nullopt);

  // Remove the last tab. The group should be removed from the model.
  tab_group_sync_service_->RemoveTab(local_group_id_1_, local_tab_id_1_);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_FALSE(group.has_value());
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabRemoved.GroupCreateOrigin", 2u);
}

TEST_F(TabGroupSyncServiceTest, ForceRemoveClosedTabGroupsOnStartup) {
  feature_list_.InitWithFeatures(
      {tab_groups::kForceRemoveClosedTabGroupsOnStartup}, {});

  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(0);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_2_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_3_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);

  model_->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});
  WaitForPostedTasks();
  // Wait again as the posted task will post again.
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, NavigateTab) {
  base::HistogramTester histogram_tester;
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title",
                                  GURL("http://www.google.com"), std::nullopt);
  WaitForPostedTasks();

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  auto* tab = group->GetTab(local_tab_id_2);
  EXPECT_TRUE(group.has_value());
  EXPECT_TRUE(tab);
  VerifyCacheGuids(*group, tab, kTestCacheGuid, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt);

  // Update tab and verify observers.
  std::u16string new_title = u"tab title 2";
  GURL new_url = GURL("http://www.example.com");

  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_2,
                                       new_url, new_title);
  WaitForPostedTasks();

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(2u, group->saved_tabs().size());

  // Verify updated tab.
  tab = group->GetTab(local_tab_id_2);
  EXPECT_TRUE(tab);
  EXPECT_EQ(new_title, tab->title());
  EXPECT_EQ(new_url, tab->url());
  VerifyCacheGuids(*group, tab, kTestCacheGuid, kTestCacheGuid, kTestCacheGuid,
                   kTestCacheGuid);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin", 1u);

  // Update redirect chain. This should not notify observers.
  SavedTabGroupTabBuilder tab_builder2;
  std::vector<GURL> redirect_url_chain({new_url, GURL("www.example.com")});
  tab_builder2.SetRedirectURLChain(redirect_url_chain);

  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(0);
  tab_group_sync_service_->UpdateTabProperties(local_group_id_1_,
                                               local_tab_id_2, tab_builder2);
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, NavigateTabIgnoresSameUrl) {
  auto local_tab_id_2 = test::GenerateRandomTabID();
  std::u16string new_title = u"tab title 2";
  GURL new_url = GURL("https://www.example.com");
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2, new_title,
                                  new_url, std::nullopt);
  WaitForPostedTasks();

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  auto* tab = group->GetTab(local_tab_id_2);
  EXPECT_TRUE(group.has_value());
  EXPECT_TRUE(tab);
  VerifyCacheGuids(*group, tab, kTestCacheGuid, kTestCacheGuid, kTestCacheGuid,
                   std::nullopt);

  // Update tab and verify observers.
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(0);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_2,
                                       new_url, new_title);
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, MoveTab) {
  base::HistogramTester histogram_tester;
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  auto* tab = group->GetTab(local_tab_id_2);
  EXPECT_EQ(1u, tab->position());

  // Move tab from position 1 to position 0.
  tab_group_sync_service_->MoveTab(local_group_id_1_, local_tab_id_2, 0);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  tab = group->GetTab(local_tab_id_2);
  EXPECT_EQ(0u, tab->position());

  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabsReordered.GroupCreateOrigin", 1u);

  // Call API with a invalid tab ID.
  tab_group_sync_service_->MoveTab(local_group_id_1_,
                                   test::GenerateRandomTabID(), 0);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabsReordered.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, OnTabSelected) {
  base::HistogramTester histogram_tester;
  // Add a new tab.
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);

  // Select tab.
  tab_group_sync_service_->OnTabSelected(local_group_id_1_, local_tab_id_2);
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabSelected.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest, RecordTabGroupEvent) {
  base::HistogramTester histogram_tester;
  EventDetails event_details(TabGroupEvent::kTabGroupOpened);
  event_details.local_tab_group_id = local_group_id_1_;
  event_details.opening_source = OpeningSource::kAutoOpenedFromSync;
  tab_group_sync_service_->RecordTabGroupEvent(event_details);
  histogram_tester.ExpectTotalCount("TabGroups.Sync.TabGroup.Opened.Reason",
                                    1u);
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
  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  model_->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, AddObserverAfterInitialize) {
  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  model_->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});
  WaitForPostedTasks();

  tab_group_sync_service_->RemoveObserver(observer_.get());

  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  tab_group_sync_service_->AddObserver(observer_.get());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedFromRemoteSource) {
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->AddedFromSync(group_4);

  // Verify that the observers are posted instead of directly notifying.
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .Times(1);
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupAddedFromLocalSource) {
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::LOCAL)))
      .Times(0);
  model_->AddedLocally(group_4);

  // Verify that the observers are posted instead of directly notifying.
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::LOCAL)))
      .Times(1);
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, EmptyGroupAddedFromLocalSource) {
  EXPECT_EQ(tab_group_sync_service_->GetAllGroups().size(), 3u);

  SavedTabGroup group_4 = test::CreateTestSavedTabGroupWithNoTabs();
  LocalTabGroupID tab_group_id = test::GenerateRandomTabGroupID();
  group_4.SetLocalGroupId(tab_group_id);

  // Add an empty group. Observers shouldn't get notified.
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::LOCAL)))
      .Times(0);
  model_->AddedLocally(group_4);
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::LOCAL)))
      .Times(0);
  WaitForPostedTasks();

  // Empty group should be excluded from GetAllGroups().
  EXPECT_EQ(tab_group_sync_service_->GetAllGroups().size(), 3u);

  // Add a tab locally. Observers should get notified.
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::LOCAL)))
      .Times(1);
  auto local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(tab_group_id, local_tab_id_2,
                                  u"random tab title", GURL("www.google.com"),
                                  std::nullopt);
  WaitForPostedTasks();

  EXPECT_EQ(tab_group_sync_service_->GetAllGroups().size(), 4u);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedFromRemoteSource) {
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(0);

  // Verify that the observers are posted instead of directly notifying.
  model_->UpdatedVisualDataFromSync(group_1_.saved_guid(), &visual_data);
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedFromLocalSource) {
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(0);

  // Verify that the observers are posted instead of directly notifying.
  model_->UpdateVisualDataLocally(group_1_.local_group_id().value(),
                                  &visual_data);
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUpdatedOnTabGroupIdMappingChange) {
  // Close a group.
  EXPECT_CALL(*observer_, OnTabGroupLocalIdChanged(Eq(group_1_.saved_guid()),
                                                   Eq(std::nullopt)))
      .Times(1);
  model_->OnGroupClosedInTabStrip(local_group_id_1_);

  // Open a group.
  LocalTabGroupID local_id_2 = test::GenerateRandomTabGroupID();
  EXPECT_CALL(*observer_, OnTabGroupLocalIdChanged(Eq(group_2_.saved_guid()),
                                                   Eq(local_id_2)))
      .Times(1);
  model_->OnGroupOpenedInTabStrip(group_2_.saved_guid(), local_id_2);
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupsReordered) {
  EXPECT_CALL(*observer_, OnTabGroupsReordered(Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->ReorderGroupLocally(group_1_.saved_guid(), 1);

  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_EQ(1, group->position());

  // Sync changes do not immediately update the positions. We use eventual
  // consistency which means we must wait for other sync position changes to
  // come in which will guarantee everything is in the right spot.
  // For this test, it is okay to keep the original position, as long as we get
  // the observer notification.
  EXPECT_CALL(*observer_, OnTabGroupsReordered(Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->ReorderGroupFromSync(group_1_.saved_guid(), 0);

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_EQ(1, group->position());
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

TEST_F(TabGroupSyncServiceTest,
       EmptyGroupsAreExcludedFromGetCallAndObserverMethods) {
  auto all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_EQ(all_groups.size(), 3u);

  // Create a group with no tabs. Observers won't be notified.
  SavedTabGroup group_4 = test::CreateTestSavedTabGroupWithNoTabs();
  base::Uuid group_id = group_4.saved_guid();
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->AddedFromSync(group_4);
  WaitForPostedTasks();

  // Verify that GetAllGroups call will not return it.
  all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_EQ(all_groups.size(), 3u);

  // Update visuals. Observers still won't be notified.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  model_->UpdatedVisualDataFromSync(group_id, &visual_data);
  WaitForPostedTasks();

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
  WaitForPostedTasks();

  // Update visuals. Observers will be notified as an Update event.
  EXPECT_CALL(*observer_,
              OnTabGroupAdded(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(0);
  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(UuidEq(group_id), Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->UpdatedVisualDataFromSync(group_id, &visual_data);
  WaitForPostedTasks();
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
  WaitForPostedTasks();

  // Remove a group with no local ID.
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_2_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->RemovedFromSync(group_2_.saved_guid());
  WaitForPostedTasks();

  // Try removing a group that doesn't exist.
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->RemovedFromSync(group_1_.saved_guid());
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupRemovedFromLocalSource) {
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  model_->RemovedLocally(group_1_.local_group_id().value());
}

TEST_F(TabGroupSyncServiceTest, GetURLRestrictionFailed) {
  GURL test_url("http://test.com/");
  optimization_guide::OptimizationMetadata metadata;

  {
    // False was returned by optimization guide.
    EXPECT_CALL(
        *decider_,
        CanApplyOptimization(
            test_url, optimization_guide::proto::SAVED_TAB_GROUP,
            An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            optimization_guide::OptimizationGuideDecision::kFalse,
            ByRef(metadata)));
    base::RunLoop run_loop;
    tab_group_sync_service_->GetURLRestriction(
        test_url,
        base::BindOnce([](std::optional<proto::UrlRestriction> restriction) {
          ASSERT_FALSE(restriction);
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  {
    // URL was not found by optimization guide.
    EXPECT_CALL(
        *decider_,
        CanApplyOptimization(
            test_url, optimization_guide::proto::SAVED_TAB_GROUP,
            An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            optimization_guide::OptimizationGuideDecision::kUnknown,
            ByRef(metadata)));
    base::RunLoop run_loop;
    tab_group_sync_service_->GetURLRestriction(
        test_url,
        base::BindOnce([](std::optional<proto::UrlRestriction> restriction) {
          ASSERT_FALSE(restriction);
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  {
    // Optimization guide returns an empty metadata.
    EXPECT_CALL(
        *decider_,
        CanApplyOptimization(
            test_url, optimization_guide::proto::SAVED_TAB_GROUP,
            An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            optimization_guide::OptimizationGuideDecision::kTrue,
            ByRef(metadata)));
    base::RunLoop run_loop;
    tab_group_sync_service_->GetURLRestriction(
        test_url,
        base::BindOnce([](std::optional<proto::UrlRestriction> restriction) {
          ASSERT_FALSE(restriction);
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  {
    // Valid response.
    proto::UrlRestriction url_restriction;
    url_restriction.set_block_for_sync(true);
    url_restriction.set_block_for_share(true);
    metadata.set_any_metadata(
        optimization_guide::AnyWrapProto(url_restriction));
    EXPECT_CALL(
        *decider_,
        CanApplyOptimization(
            test_url, optimization_guide::proto::SAVED_TAB_GROUP,
            An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            optimization_guide::OptimizationGuideDecision::kTrue,
            ByRef(metadata)));
    base::RunLoop run_loop;
    tab_group_sync_service_->GetURLRestriction(
        test_url,
        base::BindOnce([](std::optional<proto::UrlRestriction> restriction) {
          EXPECT_TRUE(restriction);
          EXPECT_TRUE(restriction->block_for_sync());
          EXPECT_TRUE(restriction->block_for_share());
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(TabGroupSyncServiceTest, UpdateTabTitleForSharedTabGroup) {
  tab_group_sync_service_->MakeTabGroupShared(local_group_id_1_, "colab");
  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();

  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));
  auto tab =
      tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_FALSE(tab.is_pending_sanitization());
  tab_group_sync_service_->UpdateLocalTabId(
      local_group_id_1_, tab.saved_tab_guid(), local_tab_id_1_);

  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       GURL("https://foo.com"), u"title2");
  tab = tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_TRUE(tab.is_pending_sanitization());
}

TEST_F(TabGroupSyncServiceTest, TabPendingSanitizationAfterMakeTabGroupShared) {
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       GURL("https://foo.com"), u"title");
  auto tab =
      tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_FALSE(tab.is_pending_sanitization());

  tab_group_sync_service_->MakeTabGroupShared(local_group_id_1_, "colab");
  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();
  tab = tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_TRUE(tab.is_pending_sanitization());
}

TEST_F(TabGroupSyncServiceTest, MakeTabGroupShared) {
  ASSERT_EQ(group_1_.saved_tabs().size(), 1u);
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());

  model_->UpdateLastUpdaterCacheGuidForGroup(
      kTestCacheGuid, local_group_id_1_,
      group_1_.saved_tabs()[0].local_tab_id());
  model_->UpdateLastUserInteractionTimeLocally(local_group_id_1_);

  // `group_1_` is a copy hence it can't be used to verify updated fields.
  std::optional<SavedTabGroup> originating_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(originating_group.has_value());
  ASSERT_FALSE(originating_group->is_shared_tab_group());

  // Verify the fields which are not expected to be copied over to the shared
  // group, apart from the local tab ID.
  ASSERT_TRUE(originating_group->position().has_value());
  ASSERT_TRUE(originating_group->creator_cache_guid().has_value());
  ASSERT_TRUE(originating_group->local_group_id().has_value());
  ASSERT_TRUE(originating_group->last_updater_cache_guid().has_value());
  ASSERT_FALSE(originating_group->last_user_interaction_time().is_null());

  for (const SavedTabGroupTab& tab : originating_group->saved_tabs()) {
    ASSERT_TRUE(tab.local_tab_id().has_value());
    ASSERT_TRUE(tab.creator_cache_guid().has_value());
    ASSERT_TRUE(tab.last_updater_cache_guid().has_value());
  }

  // Transition the saved tab group to a shared tab group, and excessively
  // verify the contents of the shared group.
  Sequence s;
  EXPECT_CALL(*coordinator_, DisconnectLocalTabGroup(local_group_id_1_))
      .InSequence(s);
  EXPECT_CALL(*coordinator_, ConnectLocalTabGroup(_, local_group_id_1_))
      .InSequence(s);
  tab_group_sync_service_->MakeTabGroupShared(local_group_id_1_,
                                              "collaboration");
  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));

  // The originating group should remain mostly unchanged.
  originating_group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  ASSERT_TRUE(originating_group.has_value());
  EXPECT_FALSE(originating_group->is_shared_tab_group());
  EXPECT_EQ(originating_group->position(), group_1_.position());
  EXPECT_EQ(originating_group->creator_cache_guid(), kTestCacheGuid);
  EXPECT_EQ(originating_group->last_updater_cache_guid(), kTestCacheGuid);
  EXPECT_FALSE(originating_group->last_user_interaction_time().is_null());

  // However the originating group should be disconnected from the local tab
  // group.
  EXPECT_EQ(originating_group->local_group_id(), std::nullopt);

  // Verify shared tab group fields.
  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group.has_value());
  EXPECT_NE(shared_group->saved_guid(), group_1_.saved_guid());
  EXPECT_TRUE(shared_group->saved_guid().is_valid());
  EXPECT_EQ(shared_group->collaboration_id(), "collaboration");
  EXPECT_EQ(shared_group->originating_saved_tab_group_guid(),
            originating_group->saved_guid());
  EXPECT_EQ(shared_group->local_group_id(), local_group_id_1_);

  // Verify that both groups have the same fields.
  EXPECT_EQ(shared_group->title(), group_1_.title());
  EXPECT_EQ(shared_group->color(), group_1_.color());

  // Verify that the shared group has updated fields.
  EXPECT_GT(shared_group->creation_time_windows_epoch_micros(),
            originating_group->creation_time_windows_epoch_micros());
  EXPECT_GT(shared_group->update_time_windows_epoch_micros(),
            originating_group->update_time_windows_epoch_micros());
  EXPECT_EQ(shared_group->creator_cache_guid(), std::nullopt);
  EXPECT_EQ(shared_group->last_updater_cache_guid(), std::nullopt);
  EXPECT_EQ(shared_group->position(), std::nullopt);
  EXPECT_TRUE(shared_group->last_user_interaction_time().is_null());

  // Verify group tabs.
  ASSERT_EQ(shared_group->saved_tabs().size(), group_1_.saved_tabs().size());
  EXPECT_FALSE(shared_group->saved_tabs().empty());
  for (size_t i = 0; i < shared_group->saved_tabs().size(); ++i) {
    const SavedTabGroupTab& shared_tab = shared_group->saved_tabs()[i];
    const SavedTabGroupTab& saved_tab = originating_group->saved_tabs()[i];

    // Verify the same fields.
    EXPECT_EQ(shared_tab.url(), saved_tab.url());
    EXPECT_EQ(shared_tab.title(), saved_tab.title());
    EXPECT_EQ(shared_tab.favicon(), saved_tab.favicon());
    EXPECT_EQ(shared_tab.saved_group_guid(), shared_group->saved_guid());

    // Verify updated fields.
    EXPECT_NE(shared_tab.saved_tab_guid(), saved_tab.saved_tab_guid());
    EXPECT_EQ(shared_tab.creator_cache_guid(), std::nullopt);
    EXPECT_NE(saved_tab.creator_cache_guid(), std::nullopt);
    EXPECT_EQ(shared_tab.last_updater_cache_guid(), std::nullopt);
    EXPECT_NE(saved_tab.last_updater_cache_guid(), std::nullopt);
    EXPECT_GT(shared_group->creation_time_windows_epoch_micros(),
              saved_tab.creation_time_windows_epoch_micros());
    EXPECT_GT(shared_tab.update_time_windows_epoch_micros(),
              saved_tab.update_time_windows_epoch_micros());

    // Do not verify the position of the original tab because its meaning
    // differs for shared tab groups: it's the index of the tab in the shared
    // group.
    EXPECT_EQ(shared_tab.position(), i);

    // Local tab ID is not covered by this test but the originating tab should
    // not have it.
    EXPECT_EQ(shared_tab.local_tab_id(), std::nullopt);
    EXPECT_EQ(saved_tab.local_tab_id(), std::nullopt);
  }
}

class PinningTabGroupSyncServiceTest : public TabGroupSyncServiceTest {
 public:
  PinningTabGroupSyncServiceTest() {
    feature_list_.InitWithFeatures({tab_groups::kTabGroupsSaveUIUpdate}, {});
  }

  PinningTabGroupSyncServiceTest(const PinningTabGroupSyncServiceTest&) =
      delete;
  PinningTabGroupSyncServiceTest& operator=(
      const PinningTabGroupSyncServiceTest&) = delete;
};

TEST_F(PinningTabGroupSyncServiceTest, UpdateGroupPositionPinnedState) {
  auto group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(group.has_value());

  const bool pinned_state = group->is_pinned();
  tab_group_sync_service_->UpdateGroupPosition(group->saved_guid(),
                                               !pinned_state, std::nullopt);
  group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_NE(group->is_pinned(), pinned_state);

  tab_group_sync_service_->UpdateGroupPosition(group->saved_guid(),
                                               pinned_state, std::nullopt);
  group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_EQ(group->is_pinned(), pinned_state);
}

TEST_F(PinningTabGroupSyncServiceTest, UpdateGroupPositionIndex) {
  auto get_index = [&](const LocalTabGroupID& local_id) -> int {
    std::vector<SavedTabGroup> groups = tab_group_sync_service_->GetAllGroups();
    auto it = base::ranges::find_if(groups, [&](const SavedTabGroup& group) {
      return group.local_group_id() == local_id;
    });

    if (it == groups.end()) {
      return -1;
    }

    return std::distance(groups.begin(), it);
  };

  std::vector<SavedTabGroup> all_groups =
      tab_group_sync_service_->GetAllGroups();
  ASSERT_EQ(3u, all_groups.size());

  tab_group_sync_service_->UpdateLocalTabGroupMapping(
      all_groups[0].saved_guid(), test::GenerateRandomTabGroupID(),
      OpeningSource::kUnknown);
  tab_group_sync_service_->UpdateLocalTabGroupMapping(
      all_groups[1].saved_guid(), test::GenerateRandomTabGroupID(),
      OpeningSource::kUnknown);
  tab_group_sync_service_->UpdateLocalTabGroupMapping(
      all_groups[2].saved_guid(), test::GenerateRandomTabGroupID(),
      OpeningSource::kUnknown);

  // Groups are inserted FILO style (like a stack data structure).
  all_groups = tab_group_sync_service_->GetAllGroups();
  const LocalTabGroupID group_id_3 = all_groups[0].local_group_id().value();
  const LocalTabGroupID group_id_2 = all_groups[1].local_group_id().value();
  const LocalTabGroupID group_id_1 = all_groups[2].local_group_id().value();

  const base::Uuid group_sync_id_3 = all_groups[0].saved_guid();
  const base::Uuid group_sync_id_1 = all_groups[2].saved_guid();

  EXPECT_EQ(0, get_index(group_id_3));
  EXPECT_EQ(1, get_index(group_id_2));
  EXPECT_EQ(2, get_index(group_id_1));

  tab_group_sync_service_->UpdateGroupPosition(group_sync_id_3, std::nullopt,
                                               2);
  EXPECT_EQ(0, get_index(group_id_2));
  EXPECT_EQ(1, get_index(group_id_1));
  EXPECT_EQ(2, get_index(group_id_3));

  tab_group_sync_service_->UpdateGroupPosition(group_sync_id_1, std::nullopt,
                                               0);
  EXPECT_EQ(0, get_index(group_id_1));
  EXPECT_EQ(1, get_index(group_id_2));
  EXPECT_EQ(2, get_index(group_id_3));

  tab_group_sync_service_->UpdateGroupPosition(group_sync_id_3, std::nullopt,
                                               1);
  EXPECT_EQ(0, get_index(group_id_1));
  EXPECT_EQ(1, get_index(group_id_3));
  EXPECT_EQ(2, get_index(group_id_2));
}

TEST_F(TabGroupSyncServiceTest, MetricsOnSignin) {
  base::HistogramTester histograms;

  identity_test_environment_.MakePrimaryAccountAvailable(
      "account@gmail.com", signin::ConsentLevel::kSignin);

  base::HistogramTester::CountsMap expected_counts{
      {"TabGroups.OnSignin.TotalTabGroupCount", 1},
      {"TabGroups.OnSignin.OpenTabGroupCount", 1},
      {"TabGroups.OnSignin.ClosedTabGroupCount", 1},
      {"TabGroups.OnSignin.TotalTabGroupTabsCount", 1},
      {"TabGroups.OnSignin.OpenTabGroupTabsCount", 1},
      {"TabGroups.OnSignin.ClosedTabGroupTabsCount", 1}};
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("TabGroups.OnSignin."),
              ContainerEq(expected_counts));

  // Sync wasn't enabled, so no "OnSync" metrics should be recorded.
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("TabGroups.OnSync."),
              IsEmpty());
}

TEST_F(TabGroupSyncServiceTest, MetricsOnSync) {
  base::HistogramTester histograms;

  identity_test_environment_.MakePrimaryAccountAvailable(
      "account@gmail.com", signin::ConsentLevel::kSync);

  // Turning on sync includes signing in, so both "OnSignin" and "OnSync"
  // metrics should be recorded.
  base::HistogramTester::CountsMap expected_signin_counts{
      {"TabGroups.OnSignin.TotalTabGroupCount", 1},
      {"TabGroups.OnSignin.OpenTabGroupCount", 1},
      {"TabGroups.OnSignin.ClosedTabGroupCount", 1},
      {"TabGroups.OnSignin.TotalTabGroupTabsCount", 1},
      {"TabGroups.OnSignin.OpenTabGroupTabsCount", 1},
      {"TabGroups.OnSignin.ClosedTabGroupTabsCount", 1}};
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("TabGroups.OnSignin."),
              ContainerEq(expected_signin_counts));

  base::HistogramTester::CountsMap expected_sync_counts{
      {"TabGroups.OnSync.TotalTabGroupCount", 1},
      {"TabGroups.OnSync.OpenTabGroupCount", 1},
      {"TabGroups.OnSync.ClosedTabGroupCount", 1},
      {"TabGroups.OnSync.TotalTabGroupTabsCount", 1},
      {"TabGroups.OnSync.OpenTabGroupTabsCount", 1},
      {"TabGroups.OnSync.ClosedTabGroupTabsCount", 1}};
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("TabGroups.OnSync."),
              ContainerEq(expected_sync_counts));
}

// Tests that after transitioning from a saved tab group to a shared tab group,
// the shared tab group is the only tab group returned by GetAllGroups().
TEST_F(TabGroupSyncServiceTest, ShouldReturnSharedTabGroupOnly) {
  ASSERT_THAT(tab_group_sync_service_->GetAllGroups(), SizeIs(3));
  ASSERT_THAT(model_->saved_tab_groups(), SizeIs(3));

  tab_group_sync_service_->MakeTabGroupShared(local_group_id_1_,
                                              "collaboration");
  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();

  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(), SizeIs(3));
  EXPECT_THAT(model_->saved_tab_groups(), SizeIs(4));
}

class EmptyTabGroupSyncServiceTest : public TabGroupSyncServiceTest {
 public:
  void MaybeInitializeTestGroups() override {}
};

TEST_F(EmptyTabGroupSyncServiceTest,
       TestModelLoadAndExtractionOfSharedTabGroupsForMessaging) {
  ASSERT_EQ(model_->Count(), 0);

  tab_group_sync_service_->SetIsInitializedForTesting(false);

  std::string collaboration_id_1 = "foo";
  std::string collaboration_id_2 = "bar";

  SavedTabGroup saved_tab_group(test::CreateTestSavedTabGroup());

  SavedTabGroup shared_group_1(test::CreateTestSavedTabGroup());
  shared_group_1.SetCollaborationId(collaboration_id_1);
  SavedTabGroup shared_group_2(test::CreateTestSavedTabGroup());
  shared_group_2.SetCollaborationId(collaboration_id_1);

  SavedTabGroup shared_group_3(test::CreateTestSavedTabGroup());
  shared_group_3.SetCollaborationId(collaboration_id_2);
  SavedTabGroup shared_group_4(test::CreateTestSavedTabGroup());
  shared_group_4.SetCollaborationId(collaboration_id_2);
  SavedTabGroup shared_group_5(test::CreateTestSavedTabGroup());
  shared_group_5.SetCollaborationId(collaboration_id_2);

  model_->LoadStoredEntries(
      /*groups=*/{saved_tab_group, shared_group_1, shared_group_2,
                  shared_group_3, shared_group_4, shared_group_5},
      /*tabs=*/{});
  task_environment_.RunUntilIdle();

  // Verify model internals.
  ASSERT_TRUE(model_->Contains(saved_tab_group.saved_guid()));
  ASSERT_TRUE(model_->Contains(shared_group_1.saved_guid()));
  ASSERT_TRUE(model_->Contains(shared_group_2.saved_guid()));
  ASSERT_TRUE(model_->Contains(shared_group_3.saved_guid()));
  ASSERT_TRUE(model_->Contains(shared_group_4.saved_guid()));
  ASSERT_TRUE(model_->Contains(shared_group_5.saved_guid()));
  ASSERT_EQ(model_->Count(), 6);

  // Retrieve groups and verify that it does not contain the saved tab group.
  std::unique_ptr<std::vector<SavedTabGroup>> shared_groups_at_startup =
      tab_group_sync_service_
          ->TakeSharedTabGroupsAvailableAtStartupForMessaging();
  EXPECT_EQ(shared_groups_at_startup.get()->size(), 5U);
  // We do not want to store this state forever, so verify that it is gone.
  EXPECT_EQ(tab_group_sync_service_
                ->TakeSharedTabGroupsAvailableAtStartupForMessaging(),
            nullptr);

  // We expect all shared groups to be present.
  std::set<base::Uuid> expected_guids = {
      shared_group_1.saved_guid(), shared_group_2.saved_guid(),
      shared_group_3.saved_guid(), shared_group_4.saved_guid(),
      shared_group_5.saved_guid()};
  for (const SavedTabGroup& shared_group : *(shared_groups_at_startup.get())) {
    EXPECT_TRUE(expected_guids.erase(shared_group.saved_guid()))
        << "Unexpected GUID " << shared_group.saved_guid()
        << " found among groups";
  }

  // Add helpful debug information in case of expectation error.
  std::vector<std::string> guid_strings;
  for (const base::Uuid& guid : expected_guids) {
    guid_strings.push_back(guid.AsLowercaseString());
  }

  // Verify that all GUIDs were found for shared tab groups.
  EXPECT_TRUE(expected_guids.empty())
      << "Not all GUIDs were found: " << base::JoinString(guid_strings, ", ");
}

}  // namespace tab_groups
