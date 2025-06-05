// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/tab_group_sync_service.h"

#include <iterator>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/logger.h"
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
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/collaboration_id.h"
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
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::An;
using testing::ByRef;
using testing::ContainerEq;
using testing::Contains;
using testing::DoAll;
using testing::Each;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Matcher;
using testing::Not;
using testing::NotNull;
using testing::Pointee;
using testing::Return;
using testing::Sequence;
using testing::SetArgPointee;
using testing::SizeIs;
using testing::WithArgs;

namespace tab_groups {
namespace {

constexpr char kTestCacheGuid[] = "test_cache_guid";
constexpr char kCollaborationId[] = "collaboration";
constexpr GaiaId::Literal kDefaultGaiaId("default_gaia_id");

MATCHER_P(HasGuid, guid, "") {
  return arg.saved_guid() == guid;
}

MATCHER(IsSharedGroup, "") {
  return arg.is_shared_tab_group();
}

MATCHER_P2(HasSharedAttribution, created_by, updated_by, "") {
  return arg.shared_attribution().created_by == GaiaId(created_by) &&
         arg.shared_attribution().updated_by == GaiaId(updated_by);
}

optimization_guide::OptimizationMetadata GetPageEntitiesMetadata(
    const std::string& title) {
  optimization_guide::proto::PageEntitiesMetadata page_entities_metadata;
  page_entities_metadata.set_alternative_title(title);
  optimization_guide::proto::Any any;
  any.set_type_url(page_entities_metadata.GetTypeName());
  page_entities_metadata.SerializeToString(any.mutable_value());
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(any);
  return metadata;
}

class MockTabGroupSyncServiceObserver : public TabGroupSyncService::Observer {
 public:
  MockTabGroupSyncServiceObserver() = default;
  ~MockTabGroupSyncServiceObserver() override = default;

  MOCK_METHOD(void, OnInitialized, ());
  MOCK_METHOD(void, OnTabGroupAdded, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupUpdated, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, BeforeTabGroupUpdateFromRemote, (const base::Uuid&));
  MOCK_METHOD(void, AfterTabGroupUpdateFromRemote, (const base::Uuid&));
  MOCK_METHOD(void, OnTabGroupRemoved, (const LocalTabGroupID&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, TriggerSource));
  MOCK_METHOD(void, OnTabSelected, (const std::set<LocalTabID>&));
  MOCK_METHOD(void,
              OnTabGroupMigrated,
              (const SavedTabGroup&, const base::Uuid&, TriggerSource));
  MOCK_METHOD(void,
              OnTabGroupLocalIdChanged,
              (const base::Uuid&, const std::optional<LocalTabGroupID>&));
  MOCK_METHOD(void, OnTabGroupsReordered, (TriggerSource));
  MOCK_METHOD(void, OnSyncBridgeUpdateTypeChanged, (SyncBridgeUpdateType));
};

class MockTabGroupSyncCoordinator : public TabGroupSyncCoordinator {
 public:
  MockTabGroupSyncCoordinator() = default;
  ~MockTabGroupSyncCoordinator() override = default;

  MOCK_METHOD(std::optional<LocalTabGroupID>,
              HandleOpenTabGroupRequest,
              (const base::Uuid&, std::unique_ptr<TabGroupActionContext>));
  MOCK_METHOD(void,
              ConnectLocalTabGroup,
              (const base::Uuid&, const LocalTabGroupID&));
  MOCK_METHOD(void, DisconnectLocalTabGroup, (const LocalTabGroupID&));
  MOCK_METHOD(std::unique_ptr<ScopedLocalObservationPauser>,
              CreateScopedLocalObserverPauser,
              ());
  MOCK_METHOD(std::set<LocalTabID>, GetSelectedTabs, ());
  MOCK_METHOD(std::u16string, GetTabTitle, (const LocalTabID&));

  MOCK_METHOD(void, OnInitialized, ());
  MOCK_METHOD(void, OnTabGroupAdded, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupUpdated, (const SavedTabGroup&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const LocalTabGroupID&, TriggerSource));
  MOCK_METHOD(void, OnTabGroupRemoved, (const base::Uuid&, TriggerSource));
  MOCK_METHOD(void,
              OnTabGroupMigrated,
              (const SavedTabGroup&, const base::Uuid&, TriggerSource));
};

class MockCollaborationFinder : public CollaborationFinder {
 public:
  MockCollaborationFinder() = default;
  ~MockCollaborationFinder() override = default;

  MOCK_METHOD(bool, IsCollaborationAvailable, (const syncer::CollaborationId&));
  MOCK_METHOD(void, SetClient, (Client*));
};

MATCHER_P(UuidEq, uuid, "") {
  return arg.saved_guid() == uuid;
}

}  // namespace

class TabGroupSyncServiceTest : public testing::Test {
 public:
  TabGroupSyncServiceTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
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
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDidSyncTabGroupsInLastSession, true);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDidEnableSharedTabGroupsInLastSession, true);
    pref_service_.registry()->RegisterDictionaryPref(prefs::kDeletedTabGroupIds,
                                                     base::Value::Dict());
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kLocallyClosedRemoteTabGroupIds, base::Value::Dict());

    auto metrics_logger =
        std::make_unique<TabGroupSyncMetricsLoggerImpl>(&device_info_tracker_);
    auto collaboration_finder =
        std::make_unique<testing::NiceMock<MockCollaborationFinder>>();
    collaboration_finder_ = collaboration_finder.get();
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
        nullptr, &pref_service_, std::move(metrics_logger), decider_.get(),
        identity_test_environment_.identity_manager(),
        std::move(collaboration_finder), /*logger=*/nullptr);
    ON_CALL(saved_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(saved_processor_, TrackedCacheGuid())
        .WillByDefault(testing::Return(kTestCacheGuid));
    ON_CALL(saved_processor_, GetControllerDelegate())
        .WillByDefault(testing::Return(fake_controller_delegate_.GetWeakPtr()));
    ON_CALL(saved_processor_, GetPossiblyTrimmedRemoteSpecifics(_))
        .WillByDefault(
            testing::ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    ON_CALL(shared_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(shared_processor_, TrackedGaiaId())
        .WillByDefault(testing::Return(kDefaultGaiaId));
    ON_CALL(shared_processor_, GetPossiblyTrimmedRemoteSpecifics(_))
        .WillByDefault(
            testing::ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    ON_CALL(*collaboration_finder_, IsCollaborationAvailable(_))
        .WillByDefault(testing::Return(true));
    ON_CALL(*decider_,
            CanApplyOptimization(
                _, optimization_guide::proto::SAVED_TAB_GROUP,
                An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillByDefault(Invoke(
            [](const GURL& url,
               optimization_guide::proto::OptimizationType optimization_type,
               optimization_guide::OptimizationGuideDecisionCallback callback) {
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideDecision::kUnknown,
                  optimization_guide::OptimizationMetadata());
            }));

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
  mock_saved_processor() {
    return &saved_processor_;
  }

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>*
  mock_shared_processor() {
    return &shared_processor_;
  }

  void TearDown() override {
    tab_group_sync_service_->RemoveObserver(observer_.get());
    model_ = nullptr;
    coordinator_ = nullptr;
    collaboration_finder_ = nullptr;
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

  void MakeTabGroupShared(const LocalTabGroupID& local_group_id,
                          std::string_view collaboration_id) {
    tab_group_sync_service_->MakeTabGroupShared(
        local_group_id, collaboration_id, base::DoNothing());

    // Simulate all shared tab groups as committed to the server.
    for (const SavedTabGroup* group : model_->GetSharedTabGroupsOnly()) {
      model_->MarkTransitionedToShared(group->saved_guid());
    }

    // Sharing a tab group is asynchronous, wait for it to complete.
    WaitForPostedTasks();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  signin::IdentityTestEnvironment identity_test_environment_;
  TestingPrefServiceSimple pref_service_;
  raw_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> saved_processor_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> shared_processor_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>
      shared_account_processor_;
  std::unique_ptr<syncer::DataTypeStore> saved_store_;
  std::unique_ptr<syncer::DataTypeStore> shared_store_;
  std::unique_ptr<syncer::DataTypeStore> shared_account_store_;
  std::unique_ptr<testing::NiceMock<MockTabGroupSyncServiceObserver>> observer_;
  raw_ptr<testing::NiceMock<MockCollaborationFinder>> collaboration_finder_;
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

TEST_F(TabGroupSyncServiceTest, GetGroupEitherId) {
  EitherGroupID either_id;

  // When holding a sync group id.
  either_id = group_1_.saved_guid();
  auto group = tab_group_sync_service_->GetGroup(either_id);
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(group->saved_guid(), group_1_.saved_guid());
  EXPECT_EQ(group->title(), group_1_.title());
  EXPECT_EQ(group->color(), group_1_.color());
  test::CompareSavedTabGroupTabs(group->saved_tabs(), group_1_.saved_tabs());

  // When holding a local group id.
  either_id = local_group_id_1_;
  group = tab_group_sync_service_->GetGroup(either_id);
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

TEST_F(TabGroupSyncServiceTest, GetTitleForPreviouslyExistingSharedTabGroup) {
  std::string collaboration_id_str = "collaboration_id";
  CollaborationId collaboration_id = CollaborationId(collaboration_id_str);

  // First ensure our test group is shared.
  MakeTabGroupShared(local_group_id_1_, collaboration_id_str);

  // Making a tab group shared changes its GUID, so we find the new GUID.
  std::optional<SavedTabGroup> shared_group_1 =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group_1.has_value());

  // Delete a group from sync. It should save the current title.
  model_->RemovedFromSync(shared_group_1->saved_guid());
  WaitForPostedTasks();

  // We should also have saved the last known title of the shared tab group.
  std::optional<std::u16string> title =
      tab_group_sync_service_->GetTitleForPreviouslyExistingSharedTabGroup(
          collaboration_id);
  ASSERT_TRUE(title.has_value());
  EXPECT_EQ(group_1_.title(), title);
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

TEST_F(TabGroupSyncServiceTest, UpdateSharedAttributionsOnUpdateVisualData) {
  MakeTabGroupShared(local_group_id_1_, "collaboration");

  EXPECT_CALL(*mock_shared_processor(), TrackedGaiaId())
      .WillOnce(Return(GaiaId("new_gaia_id")));
  tab_groups::TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  tab_group_sync_service_->UpdateVisualData(local_group_id_1_, &visual_data);

  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(group.has_value());
  EXPECT_THAT(*group, HasSharedAttribution(kDefaultGaiaId, "new_gaia_id"));
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

// Tests that adding a tab to a shared group.
TEST_F(TabGroupSyncServiceTest, AddTabToSharedGroup) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_EQ(group->saved_tabs().size(), 1u);
  MakeTabGroupShared(local_group_id_1_, "collaboration");

  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_EQ(shared_group->saved_tabs().size(), 1u);

  LocalTabID local_tab_id_2 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2, u"foo",
                                  GURL("https://www.google.com"), std::nullopt);
  LocalTabID local_tab_id_3 = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_3, u"foo2",
                                  GURL("www.google.com"), std::nullopt);

  shared_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group.has_value());
  ASSERT_EQ(shared_group->saved_tabs().size(), 3u);

  // Only tab 2 has title sanitized as it is an HTTPS url.
  EXPECT_EQ(shared_group->saved_tabs()[0].title(), u"Only Tab");
  EXPECT_EQ(shared_group->saved_tabs()[1].title(), u"google.com");
  EXPECT_EQ(shared_group->saved_tabs()[2].title(), u"foo2");

  // All tabs should have the same attribution metadata.
  EXPECT_THAT(shared_group->saved_tabs(),
              Each(HasSharedAttribution(kDefaultGaiaId, kDefaultGaiaId)));
  EXPECT_THAT(*shared_group,
              HasSharedAttribution(kDefaultGaiaId, kDefaultGaiaId));
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
#endif

TEST_F(TabGroupSyncServiceTest, CleanUpHiddenSavedTabGroupsOnStartup) {
  SavedTabGroup saved_tab_group_1(test::CreateTestSavedTabGroup());
  saved_tab_group_1.SetIsHidden(true);

  SavedTabGroup saved_tab_group_2(test::CreateTestSavedTabGroup());

  SavedTabGroup shared_group(test::CreateTestSavedTabGroup());
  CollaborationId collaboration_id("foo");
  shared_group.SetCollaborationId(collaboration_id);
  shared_group.SetIsHidden(true);

  EXPECT_CALL(*observer_, OnInitialized()).Times(1);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                saved_tab_group_1.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);

  model_->LoadStoredEntries(
      /*groups=*/{saved_tab_group_1, saved_tab_group_2, shared_group},
      /*tabs=*/{});
  task_environment_.AdvanceClock(GetOriginatingSavedGroupCleanUpTimeInterval());
  task_environment_.FastForwardBy(base::Seconds(10));
  WaitForPostedTasks();

  // Verify model internals.
  ASSERT_FALSE(model_->Contains(saved_tab_group_1.saved_guid()));
  ASSERT_TRUE(model_->Contains(saved_tab_group_2.saved_guid()));
  ASSERT_TRUE(model_->Contains(shared_group.saved_guid()));
}

TEST_F(TabGroupSyncServiceTest,
       RestoreHiddenOriginatingSavedGroupOnRemoteSharingFailure) {
  // Simulate a remote transition of `group_1_` to a shared tab group.
  ASSERT_THAT(tab_group_sync_service_->GetAllGroups(),
              Contains(HasGuid(group_1_.saved_guid())));

  SavedTabGroup shared_group =
      group_1_.CloneAsSharedTabGroup(CollaborationId(kCollaborationId));
  shared_group.MarkTransitionedToShared();
  ASSERT_FALSE(shared_group.saved_tabs().empty());
  model_->AddedFromSync(shared_group);
  WaitForPostedTasks();

  // Only `shared_group` should be available in the service.
  ASSERT_THAT(tab_group_sync_service_->GetAllGroups(),
              Contains(HasGuid(shared_group.saved_guid())));
  ASSERT_THAT(tab_group_sync_service_->GetAllGroups(),
              Not(Contains(HasGuid(group_1_.saved_guid()))));

  // Simulate a remote deletion of `shared_group`.
  model_->RemovedFromSync(shared_group.saved_guid());
  WaitForPostedTasks();

  // The originating saved tab group should be restored and available.
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Contains(HasGuid(group_1_.saved_guid())));
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

TEST_F(TabGroupSyncServiceTest, NavigateTabWithEmptyUrlRestriction) {
  feature_list_.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                                 {});
  optimization_guide::OptimizationMetadata metadata;

  // Update tab and verify observers.
  std::u16string new_title = u"tab title";
  GURL new_url = GURL("http://www.example.com");

  // False was returned by optimization guide.
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  new_url, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(metadata)));

  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       new_url, new_title);
  WaitForPostedTasks();

  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  SavedTabGroupTab* tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(new_title, tab->title());
  EXPECT_EQ(new_url, tab->url());
  VerifyCacheGuids(*group, tab, kTestCacheGuid, kTestCacheGuid, kTestCacheGuid,
                   kTestCacheGuid);
}

TEST_F(TabGroupSyncServiceTest, NavigateTabNotBlockedByUrlRestriction) {
  feature_list_.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                                 {});
  optimization_guide::OptimizationMetadata metadata;
  std::u16string title_1 = u"tab title";
  GURL url_1 = GURL("http://www.example.com#1");
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_1, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(metadata)));

  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_1, title_1);
  WaitForPostedTasks();
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  SavedTabGroupTab* tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_1, tab->title());
  EXPECT_EQ(url_1, tab->url());

  proto::UrlRestriction url_restriction;
  url_restriction.set_block_for_sync(true);
  url_restriction.set_block_for_share(true);
  url_restriction.set_block_if_only_fragment_differs(true);
  metadata.set_any_metadata(optimization_guide::AnyWrapProto(url_restriction));

  // Update tab and verify observers.
  std::u16string title_2 = u"tab title";
  GURL url_2 = GURL("http://www.foo.com");

  // True was returned by optimization guide.
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_2, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(metadata)));

  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(1);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_2, title_2);
  WaitForPostedTasks();

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_2, tab->title());
  EXPECT_EQ(url_2, tab->url());
}

TEST_F(TabGroupSyncServiceTest, NavigateTabBlockedDueToSameFragment) {
  feature_list_.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                                 {});
  optimization_guide::OptimizationMetadata metadata;
  std::u16string title_1 = u"tab title";
  GURL url_1 = GURL("http://www.example.com#1");
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_1, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(metadata)));

  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_1, title_1);
  WaitForPostedTasks();
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  SavedTabGroupTab* tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_1, tab->title());
  EXPECT_EQ(url_1, tab->url());

  proto::UrlRestriction url_restriction;
  url_restriction.set_block_for_sync(true);
  url_restriction.set_block_for_share(true);
  url_restriction.set_block_if_only_fragment_differs(true);
  metadata.set_any_metadata(optimization_guide::AnyWrapProto(url_restriction));

  // Update tab and verify observers.
  std::u16string title_2 = u"tab title 2";
  GURL url_2 = GURL("http://www.example.com#2");

  // True was returned by optimization guide.
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_2, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(metadata)));

  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(0);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_2, title_2);
  WaitForPostedTasks();

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_1, tab->title());
  EXPECT_EQ(url_1, tab->url());
}

TEST_F(TabGroupSyncServiceTest, NavigateTabUpdatesAttributionForSharedGroup) {
  MakeTabGroupShared(local_group_id_1_, "collab");

  LocalTabID local_tab_id = test::GenerateRandomTabID();
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id, u"title",
                                  GURL("http://www.google.com"), std::nullopt);

  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(group.has_value());
  ASSERT_THAT(group->GetTab(local_tab_id),
              Pointee(HasSharedAttribution(kDefaultGaiaId, kDefaultGaiaId)));

  EXPECT_CALL(*mock_shared_processor(), TrackedGaiaId())
      .WillOnce(Return(GaiaId("other_gaia_id")));
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id,
                                       GURL("http://www.example.com"),
                                       u"title 2");
  group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(group.has_value());
  EXPECT_THAT(group->GetTab(local_tab_id),
              Pointee(HasSharedAttribution(kDefaultGaiaId, "other_gaia_id")));
}

TEST_F(TabGroupSyncServiceTest, NavigateTabBlockedDueToSamePath) {
  feature_list_.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                                 {});
  optimization_guide::OptimizationMetadata metadata;
  std::u16string title_1 = u"tab title";
  GURL url_1 = GURL("http://www.example.com/xyz#1");
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_1, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(metadata)));

  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_1, title_1);
  WaitForPostedTasks();
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  SavedTabGroupTab* tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_1, tab->title());
  EXPECT_EQ(url_1, tab->url());

  proto::UrlRestriction url_restriction;
  url_restriction.set_block_for_sync(true);
  url_restriction.set_block_for_share(true);
  url_restriction.set_block_if_path_is_same(true);
  metadata.set_any_metadata(optimization_guide::AnyWrapProto(url_restriction));

  // Update tab and verify observers.
  std::u16string title_2 = u"tab title 2";
  GURL url_2 = GURL("http://www.example.com/xyz#2");

  // True was returned by optimization guide.
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_2, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(metadata)));

  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(0);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_2, title_2);
  WaitForPostedTasks();

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_1, tab->title());
  EXPECT_EQ(url_1, tab->url());
}

TEST_F(TabGroupSyncServiceTest, NavigateTabBlockedDueToSameDomain) {
  feature_list_.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                                 {});
  optimization_guide::OptimizationMetadata metadata;
  std::u16string title_1 = u"tab title";
  GURL url_1 = GURL("http://www.example.com/abc#1");
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_1, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(metadata)));

  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_1, title_1);
  WaitForPostedTasks();
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  SavedTabGroupTab* tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_1, tab->title());
  EXPECT_EQ(url_1, tab->url());

  proto::UrlRestriction url_restriction;
  url_restriction.set_block_for_sync(true);
  url_restriction.set_block_for_share(true);
  url_restriction.set_block_if_domain_is_same(true);
  metadata.set_any_metadata(optimization_guide::AnyWrapProto(url_restriction));

  // Update tab and verify observers.
  std::u16string title_2 = u"tab title 2";
  GURL url_2 = GURL("http://www.example.com/xyz#2");

  // True was returned by optimization guide.
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  url_2, optimization_guide::proto::SAVED_TAB_GROUP,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(metadata)));

  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(0);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       url_2, title_2);
  WaitForPostedTasks();

  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify updated tab.
  tab = group->GetTab(local_tab_id_1_);
  EXPECT_TRUE(tab);
  EXPECT_EQ(title_1, tab->title());
  EXPECT_EQ(url_1, tab->url());
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
  MakeTabGroupShared(local_group_id_1_, kCollaborationId);
  base::HistogramTester histogram_tester;
  base::Time test_start_time = base::Time::Now();

  // Advance the clock, so when a tab is selected, it will get a more
  // recent time than `test_start_time`.
  task_environment_.AdvanceClock(base::Seconds(5));

  // Add a new tab.
  auto local_tab_id_2 = test::GenerateRandomTabID();
  std::u16string tab_title_2 = u"random tab title";
  tab_group_sync_service_->AddTab(local_group_id_1_, local_tab_id_2,
                                  tab_title_2, GURL("www.google.com"),
                                  std::nullopt);

  {
    std::optional<SavedTabGroup> group =
        tab_group_sync_service_->GetGroup(local_group_id_1_);
    CHECK(group);

    // Local Tab 2 should start with no last_seen time.
    EXPECT_FALSE(group->GetTab(local_tab_id_2)->last_seen_time().has_value());
  }

  EXPECT_CALL(*observer_,
              OnTabSelected(Eq(std::set<LocalTabID>({local_tab_id_2}))));

  // Select tab.
  EXPECT_CALL(*coordinator_, GetSelectedTabs())
      .WillRepeatedly(Return(std::set<LocalTabID>({local_tab_id_2})));
  tab_group_sync_service_->OnTabSelected(local_group_id_1_, local_tab_id_2,
                                         tab_title_2);

  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  CHECK(group);

  // Local Tab 2 should get a last_seen time.
  const SavedTabGroupTab* tab = group->GetTab(local_tab_id_2);
  EXPECT_TRUE(tab->last_seen_time().has_value());
  EXPECT_GT(tab->last_seen_time().value(), test_start_time);

  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.TabSelected.GroupCreateOrigin", 1u);
}

TEST_F(TabGroupSyncServiceTest,
       TabGroupUpdateFromSyncWillUpdateLastSeenTimestampOfFocusedTab) {
  // Initialize a shared tab group with one tab. The tab doesn't have last seen
  // timestamp set.
  MakeTabGroupShared(local_group_id_1_, kCollaborationId);
  const SavedTabGroup* group = model_->Get(local_group_id_1_);
  CHECK(group);
  base::Uuid shared_group_id = group->saved_guid();
  const SavedTabGroupTab* tab = group->GetTab(local_tab_id_1_);
  EXPECT_FALSE(tab->last_seen_time().has_value());

  // Fake that the tab is selected.
  EXPECT_CALL(*coordinator_, GetSelectedTabs())
      .WillRepeatedly(Return(std::set<LocalTabID>({local_tab_id_1_})));

  // Update the group from sync. Since the tab is selected, it should
  // result in updating the last seen timestamp.
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(shared_group_id),
                                            Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->UpdatedVisualDataFromSync(shared_group_id, &visual_data);
  WaitForPostedTasks();

  EXPECT_TRUE(tab->last_seen_time().has_value());
}

TEST_F(TabGroupSyncServiceTest, OnTabSelectedForNonExistingTab) {
  auto local_tab_group_id_2 = test::GenerateRandomTabGroupID();
  auto local_tab_id_2 = test::GenerateRandomTabID();
  auto group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  std::u16string tab_title_2 = u"random tab title";

  EXPECT_CALL(*observer_,
              OnTabSelected(Eq(std::set<LocalTabID>({local_tab_id_2}))))
      .Times(3);

  // Select tab.
  EXPECT_CALL(*coordinator_, GetSelectedTabs())
      .WillRepeatedly(Return(std::set<LocalTabID>({local_tab_id_2})));

  tab_group_sync_service_->OnTabSelected(local_group_id_1_, local_tab_id_2,
                                         tab_title_2);
  tab_group_sync_service_->OnTabSelected(local_tab_group_id_2, local_tab_id_2,
                                         tab_title_2);
  tab_group_sync_service_->OnTabSelected(std::nullopt, local_tab_id_2,
                                         tab_title_2);
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

TEST_F(TabGroupSyncServiceTest, UpdateArchivalStatus) {
  auto group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(group.has_value());

  // Verify the archive status is defaulted to off.
  EXPECT_FALSE(group->archival_time().has_value());

  // Expect the observers to be called each time the status is updated.
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .Times(2);

  // Set the archival status and verify.
  tab_group_sync_service_->UpdateArchivalStatus(group_1_.saved_guid(), true);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  WaitForPostedTasks();
  EXPECT_TRUE(group->archival_time().has_value());

  // Reset the archival status and verify.
  tab_group_sync_service_->UpdateArchivalStatus(group_1_.saved_guid(), false);
  group = tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  WaitForPostedTasks();
  EXPECT_FALSE(group->archival_time().has_value());
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

TEST_F(TabGroupSyncServiceTest, SharedTabGroupAddedWillWaitForCollaboration) {
  EXPECT_EQ(tab_group_sync_service_->GetAllGroups().size(), 3u);

  // Create shared tab group 4 for which collaboration ID isn't yet available.
  CollaborationId collaboration_id_1("foo_1");
  SavedTabGroup group_4 = test::CreateTestSavedTabGroup();
  group_4.SetCollaborationId(collaboration_id_1);
  ON_CALL(*collaboration_finder_,
          IsCollaborationAvailable(Eq(collaboration_id_1)))
      .WillByDefault(testing::Return(false));

  // Create shared tab group 5 for which collaboration ID is already available.
  CollaborationId collaboration_id_2("foo_2");
  SavedTabGroup group_5 = test::CreateTestSavedTabGroup();
  group_5.SetCollaborationId(collaboration_id_2);
  ON_CALL(*collaboration_finder_,
          IsCollaborationAvailable(Eq(collaboration_id_2)))
      .WillByDefault(testing::Return(true));

  // Add both the groups to model from sync. Observers won't be notified for
  // group 4 but will be notified for group 5.
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .Times(0);
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_5.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .Times(1);
  model_->AddedFromSync(group_4);
  model_->AddedFromSync(group_5);
  WaitForPostedTasks();

  // GetAllGroups will skip group 4 as it's collaboration isn't ready yet.
  EXPECT_EQ(tab_group_sync_service_->GetAllGroups().size(), 4u);

  // Send an update to group 4 from sync. The update won't be notified as the
  // collaboration is pending.
  TabGroupVisualData visual_data = test::CreateTabGroupVisualData();
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_4.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .Times(0);
  model_->UpdatedVisualDataFromSync(group_4.saved_guid(), &visual_data);

  // Make the collaboration available for group 4. Observer will be notified.
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .Times(1);

  ON_CALL(*collaboration_finder_,
          IsCollaborationAvailable(Eq(collaboration_id_1)))
      .WillByDefault(testing::Return(true));
  tab_group_sync_service_->OnCollaborationAvailable(collaboration_id_1);
  WaitForPostedTasks();
  EXPECT_EQ(tab_group_sync_service_->GetAllGroups().size(), 5u);
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

  Sequence s;
  EXPECT_CALL(*observer_,
              BeforeTabGroupUpdateFromRemote(Eq(group_1_.saved_guid())))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnTabGroupUpdated(UuidEq(group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .InSequence(s);
  EXPECT_CALL(*observer_,
              AfterTabGroupUpdateFromRemote(Eq(group_1_.saved_guid())))
      .InSequence(s);
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
  EXPECT_CALL(*observer_, BeforeTabGroupUpdateFromRemote).Times(0);
  EXPECT_CALL(*observer_, AfterTabGroupUpdateFromRemote).Times(0);
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

TEST_F(TabGroupSyncServiceTest, OnSyncBridgeUpdateTypeChanged) {
  EXPECT_CALL(*observer_, OnSyncBridgeUpdateTypeChanged).Times(0);
  model_->OnSyncBridgeUpdateTypeChanged(SyncBridgeUpdateType::kDisableSync);
  testing::Mock::VerifyAndClearExpectations(observer_.get());

  // Verify that the observers are posted instead of directly notifying.
  EXPECT_CALL(*observer_, OnSyncBridgeUpdateTypeChanged(
                              Eq(SyncBridgeUpdateType::kDisableSync)))
      .Times(1);
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, TasksArePostedInTheSameSequenceAsOriginated) {
  Sequence s;
  EXPECT_CALL(*observer_, OnSyncBridgeUpdateTypeChanged(
                              Eq(SyncBridgeUpdateType::kInitialMerge)))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnTabGroupAdded(UuidEq(group_4_.saved_guid()),
                                          Eq(TriggerSource::REMOTE)))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group_1_.saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnSyncBridgeUpdateTypeChanged(
                              Eq(SyncBridgeUpdateType::kDisableSync)))
      .InSequence(s);

  model_->OnSyncBridgeUpdateTypeChanged(SyncBridgeUpdateType::kInitialMerge);
  model_->AddedFromSync(group_4_);
  model_->RemovedFromSync(group_1_.saved_guid());
  model_->OnSyncBridgeUpdateTypeChanged(SyncBridgeUpdateType::kDisableSync);
  WaitForPostedTasks();
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
        test_url, base::BindOnce([](const std::optional<proto::UrlRestriction>&
                                        restriction) {
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
        test_url, base::BindOnce([](const std::optional<proto::UrlRestriction>&
                                        restriction) {
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
        test_url, base::BindOnce([](const std::optional<proto::UrlRestriction>&
                                        restriction) {
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
        test_url, base::BindOnce([](const std::optional<proto::UrlRestriction>&
                                        restriction) {
                    EXPECT_TRUE(restriction);
                    EXPECT_TRUE(restriction->block_for_sync());
                    EXPECT_TRUE(restriction->block_for_share());
                  }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(TabGroupSyncServiceTest, SharedTabGroupTabTitleSanitizedWhenNavigate) {
  MakeTabGroupShared(local_group_id_1_, "collab");

  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));
  SavedTabGroupTab tab =
      tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  tab_group_sync_service_->UpdateLocalTabId(
      local_group_id_1_, tab.saved_tab_guid(), local_tab_id_1_);

  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       GURL("https://foo.com"), u"title2");
  tab = tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_EQ(tab.title(), u"foo.com");
}

TEST_F(TabGroupSyncServiceTest, TabTitleSanitizedAfterMakeTabGroupShared) {
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       GURL("https://foo.com"), u"title");
  MakeTabGroupShared(local_group_id_1_, "collab");

  EXPECT_EQ(
      tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs().size(),
      1u);
  SavedTabGroupTab tab =
      tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_EQ(tab.title(), u"foo.com");
}

TEST_F(TabGroupSyncServiceTest, GetTabTitleFromOptGuide) {
  feature_list_.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                                 {});
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       GURL("https://foo.com"), u"title");

  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  _, optimization_guide::proto::PAGE_ENTITIES,
                  Matcher<optimization_guide::OptimizationMetadata*>(_)))
      .WillOnce(
          DoAll(SetArgPointee<2>(GetPageEntitiesMetadata("alt1")),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));
  MakeTabGroupShared(local_group_id_1_, "collab");
  SavedTabGroupTab tab =
      tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_EQ(tab.title(), u"alt1");

  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  _, optimization_guide::proto::PAGE_ENTITIES,
                  Matcher<optimization_guide::OptimizationMetadata*>(_)))
      .WillOnce(
          DoAll(SetArgPointee<2>(GetPageEntitiesMetadata("alt2")),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));
  tab_group_sync_service_->UpdateLocalTabId(
      local_group_id_1_, tab.saved_tab_guid(), local_tab_id_1_);
  tab_group_sync_service_->NavigateTab(local_group_id_1_, local_tab_id_1_,
                                       GURL("https://foo.com"), u"title2");
  tab = tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_tabs()[0];
  EXPECT_EQ(tab.title(), u"alt2");
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

  // Advance the clock to ensure that the shared group has a different
  // creation time than the originating group.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_CALL(*observer_, OnTabGroupMigrated(_, group_1_.saved_guid(),
                                             TriggerSource::LOCAL));
  MakeTabGroupShared(local_group_id_1_, "collaboration");
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
  EXPECT_EQ(shared_group->collaboration_id(), CollaborationId("collaboration"));
  EXPECT_EQ(shared_group->GetOriginatingTabGroupGuid(),
            originating_group->saved_guid());
  EXPECT_EQ(shared_group->local_group_id(), local_group_id_1_);

  // Verify that both groups have the same fields.
  EXPECT_EQ(shared_group->title(), group_1_.title());
  EXPECT_EQ(shared_group->color(), group_1_.color());

  // Verify that the shared group has updated fields.
  EXPECT_GT(shared_group->creation_time(), originating_group->creation_time());
  EXPECT_GT(shared_group->update_time(), originating_group->update_time());
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
    EXPECT_GT(shared_group->creation_time(), saved_tab.creation_time());
    EXPECT_GT(shared_tab.update_time(), saved_tab.update_time());
    EXPECT_NE(shared_tab.local_tab_id(), std::nullopt);
    EXPECT_EQ(saved_tab.local_tab_id(), std::nullopt);

    // The local tab ID should remain the same. Use `group_1_` to verify because
    // it's a copy of the originating group before the migration.
    EXPECT_EQ(shared_tab.local_tab_id(),
              group_1_.saved_tabs()[i].local_tab_id());

    // Do not verify the position of the original tab because its meaning
    // differs for shared tab groups: it's the index of the tab in the shared
    // group.
    EXPECT_EQ(shared_tab.position(), i);
  }

  // The originating group will be removed after some time.
  EXPECT_CALL(*observer_,
              OnTabGroupRemoved(group_1_.saved_guid(), TriggerSource::LOCAL));
  task_environment_.FastForwardBy(
      GetOriginatingSavedGroupCleanUpTimeInterval());
}

TEST_F(TabGroupSyncServiceTest, ShouldRunCallbackOnMakeTabGroupShared) {
  ASSERT_EQ(group_1_.saved_tabs().size(), 1u);
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());

  base::MockCallback<TabGroupSyncService::TabGroupSharingCallback>
      mock_callback;
  EXPECT_CALL(mock_callback,
              Run(TabGroupSyncService::TabGroupSharingResult::kSuccess));

  tab_group_sync_service_->MakeTabGroupShared(
      local_group_id_1_, "collaboration", mock_callback.Get());
  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));

  // Simulate the group to be committed to the server.
  model_->MarkTransitionedToShared(
      model_->GetSharedTabGroupsOnly().front()->saved_guid());
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest,
       MakeTabGroupShared_ShouldWaitForInitialMergeCompletion) {
  ASSERT_EQ(group_1_.saved_tabs().size(), 1u);
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());

  base::MockCallback<TabGroupSyncService::TabGroupSharingCallback>
      mock_callback;

  // Mimic the state where we receive a MakeTabGroupShared call while user
  // hasn't completed sign-in.
  EXPECT_CALL(*mock_shared_processor(), TrackedGaiaId())
      .WillRepeatedly(Return(GaiaId()));
  tab_group_sync_service_->MakeTabGroupShared(
      local_group_id_1_, "collaboration", mock_callback.Get());
  WaitForPostedTasks();
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());

  // Mimic initial merge completion.
  EXPECT_CALL(*mock_shared_processor(), TrackedGaiaId())
      .WillRepeatedly(Return(GaiaId("some_gaia")));
  model_->OnSyncBridgeUpdateTypeChanged(SyncBridgeUpdateType::kDefaultState);
  WaitForPostedTasks();
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));

  // Simulate the group to be committed to the server, which will invoke the
  // client callback.
  EXPECT_CALL(mock_callback,
              Run(TabGroupSyncService::TabGroupSharingResult::kSuccess));
  model_->MarkTransitionedToShared(
      model_->GetSharedTabGroupsOnly().front()->saved_guid());
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, ShouldIgnoreUpdatesWhileTransitioningToShared) {
  ASSERT_EQ(group_1_.saved_tabs().size(), 1u);
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());

  tab_group_sync_service_->MakeTabGroupShared(
      local_group_id_1_, "collaboration", base::DoNothing());
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));

  const SavedTabGroup* shared_group = model_->GetSharedTabGroupsOnly().front();
  ASSERT_TRUE(shared_group->is_transitioning_to_shared());

  // The group should ignore any updates to the model while transitioning.
  EXPECT_CALL(*observer_, OnTabGroupUpdated).Times(0);
  model_->MergeRemoteGroupMetadata(
      shared_group->saved_guid(), u"New title", shared_group->color(),
      /*position=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*update_time=*/base::Time::Now(), /*updated_by=*/GaiaId("user_id"));
  testing::Mock::VerifyAndClearExpectations(observer_.get());

  // Once the group is transitioned, updates should be propagated.
  model_->MarkTransitionedToShared(shared_group->saved_guid());
  WaitForPostedTasks();

  EXPECT_CALL(*observer_,
              OnTabGroupUpdated(HasGuid(shared_group->saved_guid()), _));
  model_->MergeRemoteGroupMetadata(
      shared_group->saved_guid(), u"New title 2", shared_group->color(),
      /*position=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*update_time=*/base::Time::Now(), /*updated_by=*/GaiaId("user_id"));
  WaitForPostedTasks();
}

TEST_F(TabGroupSyncServiceTest, ShouldTimeoutOnMakeTabGroupShared) {
  ASSERT_EQ(group_1_.saved_tabs().size(), 1u);
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());

  base::MockCallback<TabGroupSyncService::TabGroupSharingCallback>
      mock_callback;
  EXPECT_CALL(mock_callback,
              Run(TabGroupSyncService::TabGroupSharingResult::kTimedOut));

  tab_group_sync_service_->MakeTabGroupShared(
      local_group_id_1_, "collaboration", mock_callback.Get());
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));
  WaitForPostedTasks();

  task_environment_.FastForwardBy(base::Minutes(1));
  WaitForPostedTasks();

  // The shared group should be removed from the model while the originating
  // group should remain.
  EXPECT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());
  EXPECT_THAT(model_->Get(group_1_.saved_guid()), NotNull());

  // The originating group should remain unchanged.
  ASSERT_TRUE(
      tab_group_sync_service_->GetGroup(group_1_.saved_guid()).has_value());
  ASSERT_TRUE(tab_group_sync_service_->GetGroup(local_group_id_1_).has_value());
  EXPECT_EQ(group_1_.saved_guid(),
            tab_group_sync_service_->GetGroup(local_group_id_1_)->saved_guid());
}

TEST_F(TabGroupSyncServiceTest, MakeTabGroupShared_FinishMigrationOnStartup) {
  ASSERT_EQ(group_1_.saved_tabs().size(), 1u);
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), IsEmpty());

  SavedTabGroup shared_group =
      group_1_.CloneAsSharedTabGroup(CollaborationId("collab"));
  shared_group.MarkTransitionedToShared();
  model_->AddedLocally(shared_group);

  task_environment_.FastForwardBy(base::Minutes(1));
  WaitForPostedTasks();
  ASSERT_THAT(model_->GetSharedTabGroupsOnly(), SizeIs(1));

  // The originating group should be disconnected from the local tab
  // group and become hidden.
  std::optional<SavedTabGroup> originating_group =
      tab_group_sync_service_->GetGroup(group_1_.saved_guid());
  EXPECT_TRUE(originating_group.has_value());
  EXPECT_FALSE(originating_group->is_shared_tab_group());
  EXPECT_EQ(originating_group->local_group_id(), std::nullopt);
  EXPECT_TRUE(originating_group->is_hidden());

  std::optional<SavedTabGroup> shared_tab_group =
      tab_group_sync_service_->GetGroup(shared_group.saved_guid());
  EXPECT_TRUE(shared_tab_group.has_value());
  EXPECT_TRUE(shared_tab_group->is_shared_tab_group());
  EXPECT_EQ(shared_tab_group->local_group_id(), local_group_id_1_);
  EXPECT_FALSE(shared_tab_group->is_hidden());
}

TEST_F(TabGroupSyncServiceTest, AboutToUnShareTabGroup) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  MakeTabGroupShared(local_group_id_1_, "collaboration");

  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_FALSE(shared_group->is_transitioning_to_saved());

  tab_group_sync_service_->AboutToUnShareTabGroup(local_group_id_1_,
                                                  base::DoNothing());
  shared_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_TRUE(shared_group->is_transitioning_to_saved());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUnShareFailed) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  MakeTabGroupShared(local_group_id_1_, "collaboration");

  // Unshare the tab group and fail it.
  tab_group_sync_service_->AboutToUnShareTabGroup(local_group_id_1_,
                                                  base::DoNothing());
  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_TRUE(shared_group->is_transitioning_to_saved());

  tab_group_sync_service_->OnTabGroupUnShareComplete(local_group_id_1_, false);
  shared_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_TRUE(shared_group->is_transitioning_to_saved());
}

TEST_F(TabGroupSyncServiceTest, OnTabGroupUnShareSucceeded) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  MakeTabGroupShared(local_group_id_1_, "collaboration");

  // Unshare the tab group.
  tab_group_sync_service_->AboutToUnShareTabGroup(local_group_id_1_,
                                                  base::DoNothing());
  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_TRUE(shared_group->is_transitioning_to_saved());

  // Transition the shared tab group to a saved tab group.
  Sequence s;
  EXPECT_CALL(*coordinator_, DisconnectLocalTabGroup(local_group_id_1_))
      .InSequence(s);
  EXPECT_CALL(*coordinator_, ConnectLocalTabGroup(_, local_group_id_1_))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnTabGroupMigrated(_, shared_group->saved_guid(),
                                             TriggerSource::LOCAL));

  // Advance the clock to ensure that the new saved group has a different
  // creation time than the shared group.
  task_environment_.FastForwardBy(base::Seconds(1));
  tab_group_sync_service_->OnTabGroupUnShareComplete(local_group_id_1_, true);
  shared_group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_TRUE(shared_group->is_transitioning_to_saved());

  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();

  // The originating group should have empty local group id now.
  std::optional<SavedTabGroup> originating_group =
      tab_group_sync_service_->GetGroup(shared_group->saved_guid());
  ASSERT_TRUE(originating_group.has_value());
  EXPECT_TRUE(originating_group->is_shared_tab_group());
  EXPECT_EQ(originating_group->local_group_id(), std::nullopt);

  std::optional<SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  // Verify that both groups have the same fields.
  EXPECT_EQ(shared_group->title(), saved_group->title());
  EXPECT_EQ(shared_group->color(), saved_group->color());

  // Verify that the shared group has updated fields.
  ASSERT_FALSE(saved_group->is_shared_tab_group());
  EXPECT_GT(saved_group->creation_time(), shared_group->creation_time());
  EXPECT_GT(saved_group->update_time(), shared_group->update_time());
  EXPECT_EQ(saved_group->creator_cache_guid(), kTestCacheGuid);
  EXPECT_EQ(saved_group->last_updater_cache_guid(), std::nullopt);
  EXPECT_EQ(saved_group->position(), std::nullopt);
  EXPECT_TRUE(saved_group->last_user_interaction_time().is_null());

  // Verify shared tab group fields.
  EXPECT_NE(saved_group->saved_guid(), shared_group->saved_guid());
  EXPECT_TRUE(saved_group->saved_guid().is_valid());

  EXPECT_EQ(saved_group->GetOriginatingTabGroupGuid(),
            shared_group->saved_guid());
  EXPECT_EQ(saved_group->local_group_id(), local_group_id_1_);

  // Verify group tabs.
  ASSERT_EQ(shared_group->saved_tabs().size(), group_1_.saved_tabs().size());
  EXPECT_FALSE(shared_group->saved_tabs().empty());
  for (size_t i = 0; i < shared_group->saved_tabs().size(); ++i) {
    const SavedTabGroupTab& saved_tab = saved_group->saved_tabs()[i];
    const SavedTabGroupTab& shared_tab = originating_group->saved_tabs()[i];

    // Verify the same fields.
    EXPECT_EQ(saved_tab.url(), shared_tab.url());
    EXPECT_EQ(saved_tab.title(), shared_tab.title());
    EXPECT_EQ(saved_tab.favicon(), shared_tab.favicon());
    EXPECT_EQ(saved_tab.saved_group_guid(), saved_group->saved_guid());

    // Verify updated fields.
    EXPECT_NE(saved_tab.saved_tab_guid(), shared_tab.saved_tab_guid());
    EXPECT_GT(saved_group->creation_time(), shared_tab.creation_time());
    EXPECT_GT(saved_tab.update_time(), shared_tab.update_time());
    EXPECT_NE(saved_tab.local_tab_id(), std::nullopt);
    EXPECT_EQ(shared_tab.local_tab_id(), std::nullopt);

    // Do not verify the position of the original tab because its meaning
    // differs for shared tab groups: it's the index of the tab in the shared
    // group.
    EXPECT_EQ(saved_tab.position(), i);
  }
}

TEST_F(TabGroupSyncServiceTest,
       UnShareTabGroupWhenTransitioningGroupRemovedFromSync) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  MakeTabGroupShared(local_group_id_1_, "collaboration");

  // Unshare the tab group.
  tab_group_sync_service_->AboutToUnShareTabGroup(local_group_id_1_,
                                                  base::DoNothing());
  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_TRUE(shared_group->is_transitioning_to_saved());

  // Transition the shared tab group to a saved tab group.
  Sequence s;
  EXPECT_CALL(*coordinator_, DisconnectLocalTabGroup(local_group_id_1_))
      .InSequence(s);
  EXPECT_CALL(*coordinator_, ConnectLocalTabGroup(_, local_group_id_1_))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnTabGroupMigrated(_, shared_group->saved_guid(),
                                             TriggerSource::LOCAL));

  // Advance the clock to ensure that the new saved group has a different
  // creation time than the shared group.
  task_environment_.FastForwardBy(base::Seconds(1));
  model_->RemovedFromSync(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_TRUE(shared_group->is_transitioning_to_saved());

  // The new group replaces the originating one asynchronously.
  WaitForPostedTasks();

  // The originating group should have empty local group id now.
  std::optional<SavedTabGroup> originating_group =
      tab_group_sync_service_->GetGroup(shared_group->saved_guid());
  ASSERT_TRUE(originating_group.has_value());
  EXPECT_TRUE(originating_group->is_shared_tab_group());
  EXPECT_EQ(originating_group->local_group_id(), std::nullopt);

  std::optional<SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  // Verify that both groups have the same fields.
  EXPECT_EQ(shared_group->title(), saved_group->title());
  EXPECT_EQ(shared_group->color(), saved_group->color());

  // Verify that the shared group has updated fields.
  ASSERT_FALSE(saved_group->is_shared_tab_group());
  EXPECT_GT(saved_group->creation_time(), shared_group->creation_time());
  EXPECT_GT(saved_group->update_time(), shared_group->update_time());
  EXPECT_EQ(saved_group->creator_cache_guid(), kTestCacheGuid);
  EXPECT_EQ(saved_group->last_updater_cache_guid(), std::nullopt);
  EXPECT_EQ(saved_group->position(), std::nullopt);
  EXPECT_TRUE(saved_group->last_user_interaction_time().is_null());

  // Verify shared tab group fields.
  EXPECT_NE(saved_group->saved_guid(), shared_group->saved_guid());
  EXPECT_TRUE(saved_group->saved_guid().is_valid());

  EXPECT_EQ(saved_group->GetOriginatingTabGroupGuid(),
            shared_group->saved_guid());
  EXPECT_EQ(saved_group->local_group_id(), local_group_id_1_);

  // Verify group tabs.
  ASSERT_EQ(shared_group->saved_tabs().size(), group_1_.saved_tabs().size());
  EXPECT_FALSE(shared_group->saved_tabs().empty());
  for (size_t i = 0; i < shared_group->saved_tabs().size(); ++i) {
    const SavedTabGroupTab& saved_tab = saved_group->saved_tabs()[i];
    const SavedTabGroupTab& shared_tab = originating_group->saved_tabs()[i];

    // Verify the same fields.
    EXPECT_EQ(saved_tab.url(), shared_tab.url());
    EXPECT_EQ(saved_tab.title(), shared_tab.title());
    EXPECT_EQ(saved_tab.favicon(), shared_tab.favicon());
    EXPECT_EQ(saved_tab.saved_group_guid(), saved_group->saved_guid());

    // Verify updated fields.
    EXPECT_NE(saved_tab.saved_tab_guid(), shared_tab.saved_tab_guid());
    EXPECT_GT(saved_group->creation_time(), shared_tab.creation_time());
    EXPECT_GT(saved_tab.update_time(), shared_tab.update_time());
    EXPECT_NE(saved_tab.local_tab_id(), std::nullopt);
    EXPECT_EQ(shared_tab.local_tab_id(), std::nullopt);

    // Do not verify the position of the original tab because its meaning
    // differs for shared tab groups: it's the index of the tab in the shared
    // group.
    EXPECT_EQ(saved_tab.position(), i);
  }
}

TEST_F(TabGroupSyncServiceTest, ShouldNotReturnOriginatingTabGroupOnRemoteAdd) {
  // Simulate remote transition to shared tab group from `group_1_`.
  SavedTabGroup shared_group = test::CreateTestSavedTabGroupWithNoTabs();
  shared_group.SetCollaborationId(CollaborationId("collaboration"));
  shared_group.SetOriginatingTabGroupGuid(
      group_1_.saved_guid(),
      /*use_originating_tab_group_guid=*/true);
  shared_group.SetUpdatedByAttribution(kDefaultGaiaId);

  model_->AddedFromSync(shared_group);
  WaitForPostedTasks();

  // The saved tab group should be present in GetAllGroups() while the shared
  // one is not accessible.
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Contains(HasGuid(group_1_.saved_guid())));
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Not(Contains(IsSharedGroup())));

  // Add new remote tabs to make the group available for the transition.
  model_->AddTabToGroupFromSync(
      shared_group.saved_guid(),
      test::CreateSavedTabGroupTab("http://foo.com", u"title",
                                   shared_group.saved_guid()));
  WaitForPostedTasks();

  // Only shared tab group should be returned now.
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Not(Contains(HasGuid(group_1_.saved_guid()))));
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Contains(HasGuid(shared_group.saved_guid())));
}

TEST_F(TabGroupSyncServiceTest, OnCollaborationRemoved) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_EQ(tab_group_sync_service_->GetAllGroups().size(), 3u);
  ASSERT_TRUE(model_->Contains(group->saved_guid()));

  MakeTabGroupShared(local_group_id_1_, kCollaborationId);
  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  ASSERT_TRUE(shared_group->is_shared_tab_group());
  ASSERT_EQ(tab_group_sync_service_->GetAllGroups().size(), 3u);
  ASSERT_TRUE(model_->Contains(group->saved_guid()));
  ASSERT_TRUE(model_->Contains(shared_group->saved_guid()));
  ASSERT_EQ(shared_group->saved_tabs().size(), 1u);
  SavedTabGroupTab tab = shared_group->saved_tabs()[0];

  // Observer should get 5 OnTabGroupRemoved() calls, first is the saved group,
  // then 2 comes from the shared group with guid and local group id, then 2
  // for updating UI.
  Sequence s;
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                group->saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                shared_group->saved_guid()),
                                            Eq(TriggerSource::LOCAL)))
      .InSequence(s);
  EXPECT_CALL(*observer_,
              OnTabGroupRemoved(testing::TypedEq<const LocalTabGroupID&>(
                                    shared_group->local_group_id().value()),
                                Eq(TriggerSource::LOCAL)))
      .InSequence(s);
  EXPECT_CALL(*observer_,
              OnTabGroupRemoved(testing::TypedEq<const LocalTabGroupID&>(
                                    shared_group->local_group_id().value()),
                                Eq(TriggerSource::REMOTE)))
      .InSequence(s);
  EXPECT_CALL(*observer_, OnTabGroupRemoved(testing::TypedEq<const base::Uuid&>(
                                                shared_group->saved_guid()),
                                            Eq(TriggerSource::REMOTE)))
      .InSequence(s);

  EXPECT_CALL(*mock_shared_processor(),
              UntrackEntityForStorageKey(
                  shared_group->saved_guid().AsLowercaseString()))
      .Times(1);
  EXPECT_CALL(
      *mock_shared_processor(),
      UntrackEntityForStorageKey(tab.saved_tab_guid().AsLowercaseString()))
      .Times(1);
  tab_group_sync_service_->OnCollaborationRemoved(
      syncer::CollaborationId(kCollaborationId));
  EXPECT_FALSE(tab_group_sync_service_->GetGroup(local_group_id_1_));

  EXPECT_EQ(tab_group_sync_service_->GetAllGroups().size(), 2u);
  EXPECT_FALSE(model_->Contains(group->saved_guid()));
  EXPECT_FALSE(model_->Contains(shared_group->saved_guid()));
}

TEST_F(TabGroupSyncServiceTest, OnLastSharedTabClosed) {
  std::string collaboration_id_str = "collaboration_id";
  CollaborationId collaboration_id = CollaborationId(collaboration_id_str);
  MakeTabGroupShared(local_group_id_1_, collaboration_id_str);

  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_EQ(1u, group->saved_tabs().size());
  SavedTabGroupTab tab = group->saved_tabs()[0];

  // Close the only tab in this group. One tab will be added, and the original
  // tab will be removed.
  EXPECT_CALL(*observer_,
              BeforeTabGroupUpdateFromRemote(
                  testing::TypedEq<const base::Uuid&>(group->saved_guid())));
  EXPECT_CALL(*observer_,
              AfterTabGroupUpdateFromRemote(
                  testing::TypedEq<const base::Uuid&>(group->saved_guid())));
  tab_group_sync_service_->OnLastTabClosed(
      tab_group_sync_service_->GetGroup(local_group_id_1_).value());
  group = tab_group_sync_service_->GetGroup(local_group_id_1_);
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(1u, group->saved_tabs().size());
  EXPECT_NE(tab.saved_tab_guid(), group->saved_tabs()[0].saved_tab_guid());
}

class PinningTabGroupSyncServiceTest : public TabGroupSyncServiceTest {
 public:
  PinningTabGroupSyncServiceTest() = default;
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
    auto it = std::ranges::find_if(groups, [&](const SavedTabGroup& group) {
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

  syncer::CollaborationId collaboration_id("collaboration");
  ON_CALL(*collaboration_finder_,
          IsCollaborationAvailable(Eq(collaboration_id)))
      .WillByDefault(testing::Return(true));
  MakeTabGroupShared(local_group_id_1_, collaboration_id.value());

  const std::vector<SavedTabGroup> all_groups =
      tab_group_sync_service_->GetAllGroups();
  EXPECT_THAT(all_groups, SizeIs(3));
  EXPECT_THAT(model_->saved_tab_groups(), SizeIs(4));
  EXPECT_THAT(all_groups, Not(Contains(HasGuid(group_1_.saved_guid()))));

  // The group is still accessible by ID.
  EXPECT_NE(tab_group_sync_service_->GetGroup(group_1_.saved_guid()),
            std::nullopt);
  // Simulate the case that the originating group is removed from sync after
  // the migration. It should have no impact on what is being returned from
  // GetAllGroups().
  model_->RemovedFromSync(group_1_.saved_guid());
  WaitForPostedTasks();
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(), SizeIs(3));
  EXPECT_THAT(model_->saved_tab_groups(), SizeIs(3));
}

TEST_F(TabGroupSyncServiceTest,
       RemoteAddSharedGroupWhenOriginatingGroupIsClosed) {
  // Simulate remote transition to shared tab group from `group_1_`.
  SavedTabGroup shared_group = test::CreateTestSavedTabGroupWithNoTabs();
  shared_group.SetCollaborationId(CollaborationId("collaboration"));
  shared_group.SetOriginatingTabGroupGuid(
      group_1_.saved_guid(),
      /*use_originating_tab_group_guid=*/true);
  shared_group.SetUpdatedByAttribution(kDefaultGaiaId);

  // Close the group before the shared group is added by remote.
  tab_group_sync_service_->RemoveLocalTabGroupMapping(
      local_group_id_1_, ClosingSource::kClosedByUser);
  model_->AddedFromSync(shared_group);
  WaitForPostedTasks();

  // The saved tab group should be present in GetAllGroups() while the shared
  // one is not accessible.
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Contains(HasGuid(group_1_.saved_guid())));
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Not(Contains(IsSharedGroup())));

  // Add new remote tabs to make the group available for the transition.
  model_->AddTabToGroupFromSync(
      shared_group.saved_guid(),
      test::CreateSavedTabGroupTab("http://foo.com", u"title",
                                   shared_group.saved_guid()));
  WaitForPostedTasks();

  // Only shared tab group should be returned now.
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Not(Contains(HasGuid(group_1_.saved_guid()))));
  EXPECT_THAT(tab_group_sync_service_->GetAllGroups(),
              Contains(HasGuid(shared_group.saved_guid())));
}

// Tests that saved tab group is returned if tab group migration didn't
// complete.
TEST_F(TabGroupSyncServiceTest, ShouldReturnSavedTabGroupDuringTransition) {
  ASSERT_THAT(tab_group_sync_service_->GetAllGroups(), SizeIs(3));
  ASSERT_THAT(model_->saved_tab_groups(), SizeIs(3));

  syncer::CollaborationId collaboration_id("collaboration");
  ON_CALL(*collaboration_finder_,
          IsCollaborationAvailable(Eq(collaboration_id)))
      .WillByDefault(testing::Return(true));
  tab_group_sync_service_->MakeTabGroupShared(
      local_group_id_1_, collaboration_id.value(), base::DoNothing());

  // During the transition, GetAllGroups() could also return 3 groups,
  // including the original saved group.
  std::vector<SavedTabGroup> all_groups =
      tab_group_sync_service_->GetAllGroups();
  EXPECT_THAT(all_groups, SizeIs(3));
  EXPECT_THAT(model_->saved_tab_groups(), SizeIs(4));
  EXPECT_THAT(all_groups, Contains(HasGuid(group_1_.saved_guid())));

  // Simulate all shared tab groups as committed to the server.
  for (const SavedTabGroup* group : model_->GetSharedTabGroupsOnly()) {
    model_->MarkTransitionedToShared(group->saved_guid());
  }
  WaitForPostedTasks();

  // Once committed, GetAllGroups() should still return 3 groups but a
  // shared group instead of the original saved group.
  all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_THAT(all_groups, SizeIs(3));
  EXPECT_THAT(model_->saved_tab_groups(), SizeIs(4));
  EXPECT_THAT(all_groups, Not(Contains(HasGuid(group_1_.saved_guid()))));
}

// Tests that after transitioning from a shared tab group to a saved tab group,
// the saved tab group is the only tab group returned by GetAllGroups().
TEST_F(TabGroupSyncServiceTest, ShouldReturnSavedTabGroupOnly) {
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);
  MakeTabGroupShared(local_group_id_1_, "collaboration");
  ASSERT_THAT(tab_group_sync_service_->GetAllGroups(), SizeIs(3));
  ASSERT_THAT(model_->saved_tab_groups(), SizeIs(4));
  ASSERT_TRUE(model_->Contains(group->saved_guid()));
  std::optional<SavedTabGroup> shared_group =
      tab_group_sync_service_->GetGroup(local_group_id_1_);

  // Unshare the tab group.
  tab_group_sync_service_->AboutToUnShareTabGroup(local_group_id_1_,
                                                  base::DoNothing());
  tab_group_sync_service_->OnTabGroupUnShareComplete(local_group_id_1_, true);

  // During the transition, GetAllGroups() should return 3 groups, including
  // the original shared group.
  std::vector<SavedTabGroup> all_groups =
      tab_group_sync_service_->GetAllGroups();
  EXPECT_THAT(all_groups, SizeIs(3));
  EXPECT_THAT(model_->saved_tab_groups(), SizeIs(4));
  EXPECT_THAT(all_groups, Contains(HasGuid(shared_group->saved_guid())));
  EXPECT_FALSE(model_->Contains(group->saved_guid()));

  WaitForPostedTasks();
  all_groups = tab_group_sync_service_->GetAllGroups();
  EXPECT_THAT(all_groups, SizeIs(3));
  EXPECT_THAT(model_->saved_tab_groups(), SizeIs(4));
  EXPECT_THAT(all_groups, Not(Contains(HasGuid(shared_group->saved_guid()))));
}

class EmptyTabGroupSyncServiceTest : public TabGroupSyncServiceTest {
 public:
  void MaybeInitializeTestGroups() override {}
};

TEST_F(EmptyTabGroupSyncServiceTest,
       TestModelLoadAndExtractionOfSharedTabGroupsForMessaging) {
  ASSERT_EQ(model_->Count(), 0);

  tab_group_sync_service_->SetIsInitializedForTesting(false);

  CollaborationId collaboration_id_1("foo");
  CollaborationId collaboration_id_2("bar");

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
