// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tab_groups/tab_group_color.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceClosure;

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;

namespace collaboration::messaging {

namespace {

tab_groups::SavedTabGroupTab CreateSavedTabGroupTab(
    const std::string& url,
    const std::u16string& title,
    const base::Uuid& group_guid,
    std::optional<int> position = std::nullopt) {
  auto tab = tab_groups::test::CreateSavedTabGroupTab(url, title, group_guid,
                                                      position);
  tab.SetLocalTabID(tab_groups::test::GenerateRandomTabID());
  return tab;
}

bool TabGroupsHaveSameGuidAndType(tab_groups::SavedTabGroup a,
                                  tab_groups::SavedTabGroup b) {
  return a.is_shared_tab_group() == b.is_shared_tab_group() &&
         a.saved_guid() == b.saved_guid();
}

MATCHER_P(TabGroupGuidEq, expected_group, "") {
  return TabGroupsHaveSameGuidAndType(arg, expected_group);
}

bool TabsHaveSameGuid(tab_groups::SavedTabGroupTab a,
                      tab_groups::SavedTabGroupTab b) {
  return a.saved_tab_guid() == b.saved_tab_guid();
}

MATCHER_P(TabGuidEq, expected_tab, "") {
  return TabsHaveSameGuid(arg, expected_tab);
}

tab_groups::SavedTabGroup CreateTestSharedTabGroup() {
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetCollaborationId(tab_groups::CollaborationId("collab_id"));
  return group;
}

tab_groups::SavedTabGroup CreateTestSharedTabGroupWithNoTabs() {
  tab_groups::SavedTabGroup group =
      tab_groups::test::CreateTestSavedTabGroupWithNoTabs();
  group.SetCollaborationId(tab_groups::CollaborationId("collab_id"));
  return group;
}
}  // namespace

class MockTabGroupChangeNotifierObserver
    : public TabGroupChangeNotifier::Observer {
 public:
  MockTabGroupChangeNotifierObserver() = default;
  ~MockTabGroupChangeNotifierObserver() override = default;

  MOCK_METHOD(void, OnTabGroupChangeNotifierInitialized, ());
  MOCK_METHOD(void, OnSyncDisabled, ());
  MOCK_METHOD(void,
              OnTabGroupAdded,
              (const tab_groups::SavedTabGroup&, tab_groups::TriggerSource));
  MOCK_METHOD(void,
              OnTabGroupRemoved,
              (tab_groups::SavedTabGroup, tab_groups::TriggerSource));
  MOCK_METHOD(void,
              OnTabGroupNameUpdated,
              (const tab_groups::SavedTabGroup&, tab_groups::TriggerSource));
  MOCK_METHOD(void,
              OnTabGroupColorUpdated,
              (const tab_groups::SavedTabGroup&, tab_groups::TriggerSource));
  MOCK_METHOD(void,
              OnTabAdded,
              (const tab_groups::SavedTabGroupTab&, tab_groups::TriggerSource));
  MOCK_METHOD(void,
              OnTabRemoved,
              (tab_groups::SavedTabGroupTab, tab_groups::TriggerSource, bool));
  MOCK_METHOD(void,
              OnTabUpdated,
              (const tab_groups::SavedTabGroupTab&,
               const tab_groups::SavedTabGroupTab&,
               tab_groups::TriggerSource,
               bool));
  MOCK_METHOD(void,
              OnTabSelectionChanged,
              (const tab_groups::LocalTabID&, bool));
  MOCK_METHOD(void,
              OnTabLastSeenTimeChanged,
              (const base::Uuid& tab_id, tab_groups::TriggerSource source));
  MOCK_METHOD(void,
              OnTabGroupOpened,
              (const tab_groups::SavedTabGroup& tab_group));
  MOCK_METHOD(void,
              OnTabGroupClosed,
              (const tab_groups::SavedTabGroup& tab_group));
};

class TabGroupChangeNotifierImplTest : public testing::Test {
 public:
  TabGroupChangeNotifierImplTest() = default;
  ~TabGroupChangeNotifierImplTest() override = default;

  void SetUp() override {
    tab_group_sync_service_ =
        std::make_unique<tab_groups::MockTabGroupSyncService>();
    notifier_ = std::make_unique<TabGroupChangeNotifierImpl>(
        tab_group_sync_service_.get(), identity_test_env_.identity_manager());
    AddNotifierObserver();
  }

  void InitializeNotifier(
      std::vector<tab_groups::SavedTabGroup> startup_tab_groups,
      std::vector<tab_groups::SavedTabGroup> init_tab_groups) {
    EXPECT_CALL(*tab_group_sync_service_,
                TakeSharedTabGroupsAvailableAtStartupForMessaging())
        .WillOnce(
            Return(std::make_unique<std::vector<tab_groups::SavedTabGroup>>(
                std::move(startup_tab_groups))));
    EXPECT_CALL(*tab_group_sync_service_, AddObserver(_))
        .WillOnce([&](tab_groups::TabGroupSyncService::Observer* observer) {
          tgss_observer_ = observer;
          // The TabGroupSyncService currently uses re-entrancy when informing
          // its observers about its initialization state. We are mimicking that
          // behavior here.
          observer->OnInitialized();
        });
    EXPECT_CALL(*tab_group_sync_service_, GetAllGroups())
        .WillOnce(Return(init_tab_groups));
    base::RunLoop run_loop;
    EXPECT_CALL(*notifier_observer_, OnTabGroupChangeNotifierInitialized())
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    notifier_->Initialize();
    run_loop.Run();
  }

  void AddNotifierObserver() {
    notifier_observer_ = std::make_unique<MockTabGroupChangeNotifierObserver>();
    notifier_->AddObserver(notifier_observer_.get());
  }

  void MaybeRemoveNotifierObserver() {
    if (!notifier_observer_) {
      return;
    }

    notifier_->RemoveObserver(notifier_observer_.get());
  }

  // For tab updates that should be considered updates, this returns the updated
  // tab received from an observer.
  tab_groups::SavedTabGroupTab GetUpdatedTab(
      const tab_groups::SavedTabGroup& old_group,
      tab_groups::SavedTabGroupTab updated_tab) {
    tab_groups::SavedTabGroup updated_tab_group = old_group;
    updated_tab_group.UpdateTab(updated_tab);
    // This tab will be overridden by the callback.
    tab_groups::SavedTabGroupTab tab_update_received =
        CreateSavedTabGroupTab("N/A", u"N/A", updated_tab_group.saved_guid());
    EXPECT_CALL(*notifier_observer_,
                OnTabUpdated(_, TabGuidEq(updated_tab),
                             Eq(tab_groups::TriggerSource::REMOTE), Eq(false)))
        .WillOnce(SaveArg<1>(&tab_update_received));
    UpdateTabGroup(updated_tab_group, tab_groups::TriggerSource::REMOTE);
    return tab_update_received;
  }

  void VerifyTabNotUpdated(const tab_groups::SavedTabGroup& old_group,
                           tab_groups::SavedTabGroupTab changed_tab) {
    tab_groups::SavedTabGroup updated_tab_group = old_group;
    updated_tab_group.UpdateTab(changed_tab);
    EXPECT_CALL(*notifier_observer_, OnTabUpdated).Times(0);
  }

  void TearDown() override {
    EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_)).Times(1);
    MaybeRemoveNotifierObserver();
  }

  void UpdateTabGroup(const tab_groups::SavedTabGroup& tab_group,
                      tab_groups::TriggerSource source) {
    // We expect the notifier to use live data on the posted task.
    base::Uuid group_guid = tab_group.saved_guid();
    EXPECT_CALL(*tab_group_sync_service_, GetGroup(group_guid))
        .WillRepeatedly(Return(tab_group));
    tgss_observer_->BeforeTabGroupUpdateFromRemote(group_guid);
    tgss_observer_->OnTabGroupUpdated(tab_group, source);
    tgss_observer_->AfterTabGroupUpdateFromRemote(group_guid);

    // Post a dummy task in the current thread and wait for its completion so
    // that the posted task is completed.
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  std::unique_ptr<tab_groups::MockTabGroupSyncService> tab_group_sync_service_;
  std::unique_ptr<TabGroupChangeNotifierImpl> notifier_;
  std::unique_ptr<MockTabGroupChangeNotifierObserver> notifier_observer_;

  raw_ptr<tab_groups::TabGroupSyncService::Observer> tgss_observer_;
};

TEST_F(TabGroupChangeNotifierImplTest, TestInitialization) {
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());
  EXPECT_EQ(true, notifier_->IsInitialized());
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupsAddedAndRemoved) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  // Add a tab group to the service and ensure the observer is informed.
  tab_groups::SavedTabGroup tab_group_1 = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_1),
                              Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabGroupAdded(tab_group_1,
                                  tab_groups::TriggerSource::REMOTE);

  // Add another test group and ensure the observer is informed.
  tab_groups::SavedTabGroup tab_group_2 = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_2),
                              Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabGroupAdded(tab_group_2,
                                  tab_groups::TriggerSource::REMOTE);

  // Remove the first group and ensure the observer is informed.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(tab_group_1),
                                Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabGroupRemoved(tab_group_1.saved_guid(),
                                    tab_groups::TriggerSource::REMOTE);

  // Remove the second group and ensure the observer is informed.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(tab_group_2),
                                Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabGroupRemoved(tab_group_2.saved_guid(),
                                    tab_groups::TriggerSource::REMOTE);
}

TEST_F(TabGroupChangeNotifierImplTest,
       TestTabGroupsAddedAndRemoved_IgnoredWhenInitialMergeNotCompleted) {
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  // Sign-in and start initial merge. Incoming sync updates should be ignored.
  EXPECT_CALL(*notifier_observer_, OnSyncDisabled).Times(0);
  tgss_observer_->OnSyncBridgeUpdateTypeChanged(
      tab_groups::SyncBridgeUpdateType::kInitialMerge);
  // Add a tab group to the service.
  tab_groups::SavedTabGroup tab_group_1 = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded).Times(0);
  tgss_observer_->OnTabGroupAdded(tab_group_1,
                                  tab_groups::TriggerSource::REMOTE);
  testing::Mock::VerifyAndClearExpectations(tgss_observer_);

  // Complete initial merge.
  EXPECT_CALL(*notifier_observer_, OnSyncDisabled).Times(0);
  tgss_observer_->OnSyncBridgeUpdateTypeChanged(
      tab_groups::SyncBridgeUpdateType::kDefaultState);

  // Add another test group and ensure the observer is informed.
  tab_groups::SavedTabGroup tab_group_2 = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_2),
                              Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabGroupAdded(tab_group_2,
                                  tab_groups::TriggerSource::REMOTE);
  testing::Mock::VerifyAndClearExpectations(tgss_observer_);

  // Sign-out and start disabling sync. Incoming sync updates should be ignored.
  // Remove the first group and ensure the observer is not informed.
  EXPECT_CALL(*notifier_observer_, OnSyncDisabled).Times(1);
  tgss_observer_->OnSyncBridgeUpdateTypeChanged(
      tab_groups::SyncBridgeUpdateType::kDisableSync);

  EXPECT_CALL(*notifier_observer_, OnTabGroupRemoved).Times(0);
  tgss_observer_->OnTabGroupRemoved(tab_group_1.saved_guid(),
                                    tab_groups::TriggerSource::REMOTE);
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupsAvailableOnStartup) {
  tab_groups::SavedTabGroup tab_group_1 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_2 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_2_title_changed = tab_group_2;
  tab_group_2_title_changed.SetTitle(tab_group_2.title() + u"_changed");
  tab_groups::SavedTabGroup tab_group_3 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_3_color_changed = tab_group_3;
  // Original color is `kBlue`.
  tab_group_3_color_changed.SetColor(tab_groups::TabGroupColorId::kOrange);
  tab_groups::SavedTabGroup tab_group_4 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_4_title_and_color_changed = tab_group_4;
  tab_group_4_title_and_color_changed.SetTitle(tab_group_4.title() +
                                               u"_changed");
  // Original color is `kBlue`.
  tab_group_4_title_and_color_changed.SetColor(
      tab_groups::TabGroupColorId::kOrange);
  tab_groups::SavedTabGroup tab_group_5 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_6 = CreateTestSharedTabGroup();

  // On startup, 5 tab groups are available.
  std::vector<tab_groups::SavedTabGroup> startup_tab_groups;
  startup_tab_groups.emplace_back(tab_group_1);  // Will be removed.
  startup_tab_groups.emplace_back(tab_group_2);  // Will have title change.
  startup_tab_groups.emplace_back(tab_group_3);  // Will have color change.
  startup_tab_groups.emplace_back(
      tab_group_4);  // Will have title and color changes.
  startup_tab_groups.emplace_back(tab_group_5);  // Will be unchanged.

  // At initialization time, one of the initial tab groups has been removed, a
  // new one has been added, one has a title change, one has a color change, one
  // has both title and color changes, and one is unchanged.
  std::vector<tab_groups::SavedTabGroup> init_tab_groups;
  init_tab_groups.emplace_back(tab_group_2_title_changed);
  init_tab_groups.emplace_back(tab_group_3_color_changed);
  init_tab_groups.emplace_back(tab_group_4_title_and_color_changed);
  init_tab_groups.emplace_back(tab_group_5);  // Unchanged.
  init_tab_groups.emplace_back(tab_group_6);  // New.

  // Ensure the observer is informed of all changes.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(tab_group_1),
                                Eq(tab_groups::TriggerSource::REMOTE)));
  // Capture the updated tab groups so we can verify we received the updates.
  // The created test groups will be overwritten from the SaveArg, but we don't
  // have an empty constructor.
  // `tab_group_2` has title change.
  tab_groups::SavedTabGroup tab_group_2_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_2_title_changed),
                                    Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_2_received));
  // `tab_group_3` has color change.
  tab_groups::SavedTabGroup tab_group_3_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_3_color_changed),
                                     Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_3_received));
  // `tab_group_4` has both title and color change.
  tab_groups::SavedTabGroup tab_group_4_received_title =
      CreateTestSharedTabGroup();
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_4_title_and_color_changed),
                            Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_4_received_title));
  tab_groups::SavedTabGroup tab_group_4_received_color =
      CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(
                  TabGroupGuidEq(tab_group_4_title_and_color_changed),
                  Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_4_received_color));
  // New group.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_6),
                              Eq(tab_groups::TriggerSource::REMOTE)));

  // Ensure we can capture all the arguments of the callbacks using
  // RunUntilIdle. Since we are using a runloop, we will also be informed about
  // initialization being finished.
  InitializeNotifier(startup_tab_groups, init_tab_groups);

  // Verify that we received the updated titles and colors instead of the old
  // one.
  EXPECT_EQ(tab_group_2_title_changed.title(), tab_group_2_received.title());
  EXPECT_EQ(tab_group_3_color_changed.color(), tab_group_3_received.color());
  EXPECT_EQ(tab_group_4_title_and_color_changed.title(),
            tab_group_4_received_title.title());
  EXPECT_EQ(tab_group_4_title_and_color_changed.color(),
            tab_group_4_received_color.color());
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupsAddedLocally) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());
  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroup();

  // Local created groups should be published.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(_, Eq(tab_groups::TriggerSource::LOCAL)))
      .Times(1);
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::LOCAL);

  // Verify that we get informed about remote color/title changes for this
  // group.
  tab_groups::SavedTabGroup tab_group_received = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_title_changed = tab_group;
  tab_group_title_changed.SetTitle(tab_group.title() + u"_changed");
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_title_changed),
                                    Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  UpdateTabGroup(tab_group_title_changed, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group_title_changed.title());
  EXPECT_EQ(tab_group_received.color(), tab_group_title_changed.color());

  // Verify that we get informed about local color/title changes for this group.
  tab_groups::SavedTabGroup tab_group_color_changed = tab_group_title_changed;
  // Original color is `kBlue`.
  tab_group_color_changed.SetColor(tab_groups::TabGroupColorId::kOrange);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_color_changed),
                                     Eq(tab_groups::TriggerSource::LOCAL)))
      .Times(1);
  UpdateTabGroup(tab_group_color_changed, tab_groups::TriggerSource::LOCAL);

  // Local deletes of groups should be published.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(_, Eq(tab_groups::TriggerSource::LOCAL)))
      .Times(1);
  tgss_observer_->OnTabGroupRemoved(tab_group.saved_guid(),
                                    tab_groups::TriggerSource::LOCAL);
}

TEST_F(TabGroupChangeNotifierImplTest, TestIgnoreSavedTabGroups) {
  tab_groups::SavedTabGroup saved_tab_group_1 =
      tab_groups::test::CreateTestSavedTabGroup();
  tab_groups::SavedTabGroup saved_tab_group_2 =
      tab_groups::test::CreateTestSavedTabGroup();
  tab_groups::SavedTabGroup shared_tab_group = CreateTestSharedTabGroup();

  // We should only be informed about the shared tab group after startup.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(shared_tab_group),
                              Eq(tab_groups::TriggerSource::REMOTE)))
      .Times(1);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(saved_tab_group_1), _))
      .Times(0);
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(
          {saved_tab_group_1, shared_tab_group}));

  // Any saved tab groups added at runtime should be ignored.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(saved_tab_group_2), _))
      .Times(0);
  tgss_observer_->OnTabGroupAdded(saved_tab_group_2,
                                  tab_groups::TriggerSource::REMOTE);

  // Verify title updates are not published.
  tab_groups::SavedTabGroup saved_tab_group_1_title_changed = saved_tab_group_1;
  saved_tab_group_1_title_changed.SetTitle(saved_tab_group_1.title() +
                                           u"_changed");
  UpdateTabGroup(saved_tab_group_1_title_changed,
                 tab_groups::TriggerSource::REMOTE);
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupNameUpdated(TabGroupGuidEq(saved_tab_group_1_title_changed), _))
      .Times(0);

  // Verify color updates are not published.
  tab_groups::SavedTabGroup saved_tab_group_2_color_changed = saved_tab_group_2;
  // Original coltitle mr is `kBlue`.
  saved_tab_group_2_color_changed.SetColor(
      tab_groups::TabGroupColorId::kOrange);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(
                  TabGroupGuidEq(saved_tab_group_2_color_changed), _))
      .Times(0);

  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(saved_tab_group_1), _))
      .Times(0);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(saved_tab_group_2), _))
      .Times(0);
  tgss_observer_->OnTabGroupRemoved(saved_tab_group_1.saved_guid(),
                                    tab_groups::TriggerSource::REMOTE);
  tgss_observer_->OnTabGroupRemoved(saved_tab_group_2.saved_guid(),
                                    tab_groups::TriggerSource::REMOTE);
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupUpdatedBecomesAdded) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  // Inform our listener that a tab was updated, but it is unknown to us, we
  // should then inform our observers that it was added.
  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group),
                              Eq(tab_groups::TriggerSource::REMOTE)));
  UpdateTabGroup(tab_group, tab_groups::TriggerSource::REMOTE);
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupUpdated) {
  tab_groups::SavedTabGroup tab_group_1 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_1_title_changed = tab_group_1;
  tab_group_1_title_changed.SetTitle(tab_group_1.title() + u"_changed");
  tab_groups::SavedTabGroup tab_group_2 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_2_color_changed = tab_group_2;
  // Original color is `kBlue`.
  tab_group_2_color_changed.SetColor(tab_groups::TabGroupColorId::kOrange);
  tab_groups::SavedTabGroup tab_group_3 = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_3_title_and_color_changed = tab_group_3;
  tab_group_3_title_and_color_changed.SetTitle(tab_group_3.title() +
                                               u"_changed");
  // Original color is `kBlue`.
  tab_group_3_title_and_color_changed.SetColor(
      tab_groups::TabGroupColorId::kOrange);

  // We will initially get three calls to OnTabGroupAdded.
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded).Times(3);

  // Initialize the notifier with an empty set of tab groups available on
  // startup and one on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(
          {tab_group_1, tab_group_2, tab_group_3}));

  // Verify title change.
  // This will be overridden by SaveArg.
  tab_groups::SavedTabGroup tab_group_1_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_1_title_changed),
                                    Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_1_received));
  UpdateTabGroup(tab_group_1_title_changed, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_1_title_changed.title(), tab_group_1_received.title());

  // Verify color change.
  // This will be overridden by SaveArg.
  tab_groups::SavedTabGroup tab_group_2_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_2_color_changed),
                                     Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_2_received));
  UpdateTabGroup(tab_group_2_color_changed, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_2_color_changed.color(), tab_group_2_received.color());

  // Verify title and color change.
  // These will be overridden by SaveArg.
  tab_groups::SavedTabGroup tab_group_3_received_name =
      CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_3_received_color =
      CreateTestSharedTabGroup();
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_3_title_and_color_changed),
                            Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_3_received_name));
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(
                  TabGroupGuidEq(tab_group_3_title_and_color_changed),
                  Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_3_received_color));
  UpdateTabGroup(tab_group_3_title_and_color_changed,
                 tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_3_title_and_color_changed.title(),
            tab_group_3_received_name.title());
  EXPECT_EQ(tab_group_3_title_and_color_changed.color(),
            tab_group_3_received_color.color());
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupAddedUpdatedRemoved) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  // Create a chained scenario of a tab group being added, then updating its
  // title, then color, and then both, before it is removed.
  // We want to verify that the newest version is always provided and used
  // as delta for the next one.
  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_title_changed = tab_group;
  tab_group_title_changed.SetTitle(tab_group.title() + u"_changed");
  tab_groups::SavedTabGroup tab_group_color_changed = tab_group_title_changed;
  // Original color is `kBlue`.
  tab_group_color_changed.SetColor(tab_groups::TabGroupColorId::kOrange);
  tab_groups::SavedTabGroup tab_group_title_and_color_changed =
      tab_group_color_changed;
  tab_group_title_and_color_changed.SetTitle(tab_group_color_changed.title() +
                                             u"_changed");
  tab_group_title_and_color_changed.SetColor(
      tab_groups::TabGroupColorId::kYellow);

  // First add the group
  tab_groups::SavedTabGroup tab_group_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group),
                              Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group.title());
  EXPECT_EQ(tab_group_received.color(), tab_group.color());

  // Then update the title.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_title_changed),
                                    Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  UpdateTabGroup(tab_group_title_changed, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group_title_changed.title());
  EXPECT_EQ(tab_group_received.color(), tab_group_title_changed.color());

  // Then update the color.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_color_changed),
                                     Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  UpdateTabGroup(tab_group_color_changed, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group_color_changed.title());
  EXPECT_EQ(tab_group_received.color(), tab_group_color_changed.color());

  // Then update both title and color.
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_title_and_color_changed),
                            Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_title_and_color_changed),
                             Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  UpdateTabGroup(tab_group_title_and_color_changed,
                 tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(),
            tab_group_title_and_color_changed.title());
  EXPECT_EQ(tab_group_received.color(),
            tab_group_title_and_color_changed.color());

  // Then remove the group.
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupRemoved(TabGroupGuidEq(tab_group_title_and_color_changed),
                        Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  tgss_observer_->OnTabGroupRemoved(
      tab_group_title_and_color_changed.saved_guid(),
      tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(),
            tab_group_title_and_color_changed.title());
  EXPECT_EQ(tab_group_received.color(),
            tab_group_title_and_color_changed.color());
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupTabUpdatesAtStartup) {
  tab_groups::SavedTabGroup tab_group_startup =
      CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("url1", u"title1", tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab2 = CreateSavedTabGroupTab(
      "https://example.com/", u"title2", tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab3 =
      CreateSavedTabGroupTab("url3", u"title3", tab_group_startup.saved_guid());
  tab_group_startup.AddTabFromSync(tab1);
  tab_group_startup.AddTabFromSync(tab2);
  tab_group_startup.AddTabFromSync(tab3);

  // At init tab4 was added, tab2 was updated, and tab1 was removed.
  tab_groups::SavedTabGroup tab_group_init = tab_group_startup;
  tab_group_init.RemoveTabFromSync(tab1.saved_tab_guid(),
                                   /*removed_by=*/GaiaId());
  tab_groups::SavedTabGroupTab tab2_updated = tab2;
  tab2_updated.SetURL(GURL("https://example.com/subpage/"));
  tab_group_init.UpdateTab(tab2_updated);
  tab_groups::SavedTabGroupTab tab4 =
      CreateSavedTabGroupTab("url4", u"title4", tab_group_init.saved_guid());
  tab_group_init.AddTabFromSync(tab4);

  // These tabs will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab_received_added =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab_received_updated =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab_received_removed =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group_startup.saved_guid());
  EXPECT_CALL(
      *notifier_observer_,
      OnTabAdded(TabGuidEq(tab4), Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_received_added));

  tab_groups::SavedTabGroupTab tab_received_before =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group_startup.saved_guid());
  EXPECT_CALL(*notifier_observer_,
              OnTabUpdated(TabGuidEq(tab2), TabGuidEq(tab2),
                           Eq(tab_groups::TriggerSource::REMOTE), Eq(false)))
      .WillOnce(DoAll(SaveArg<0>(&tab_received_before),
                      SaveArg<1>(&tab_received_updated)));

  EXPECT_CALL(*notifier_observer_,
              OnTabRemoved(TabGuidEq(tab1),
                           Eq(tab_groups::TriggerSource::REMOTE), Eq(false)))
      .WillOnce(SaveArg<0>(&tab_received_removed));

  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(
          {tab_group_startup}),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(
          {tab_group_init}));

  EXPECT_EQ(tab2_updated.url(), tab_received_updated.url());
  EXPECT_EQ(tab2.url(), tab_received_before.url());

  // Now, ensure that the latest update of the tab is used and that going back
  // to the original URL is considered an update again.
  tab_groups::SavedTabGroupTab tab2_restored = tab2_updated;
  tab2_restored.SetURL(GURL("https://www.example.com/"));
  tab_groups::SavedTabGroupTab tab2_restored_received =
      GetUpdatedTab(tab_group_init, tab2_restored);
  EXPECT_EQ(tab2_restored.url(), tab2_restored_received.url());
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabGroupTabUpdatesAtRuntime) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("url1", u"title1", tab_group.saved_guid());
  tab_groups::SavedTabGroupTab tab2 = CreateSavedTabGroupTab(
      "https://www.example.com/", u"title2", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab1);
  tab_group.AddTabFromSync(tab2);

  // First add the group.
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded);
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);

  // Add a new tab to the group and update it.
  tab_groups::SavedTabGroupTab tab3 =
      CreateSavedTabGroupTab("url3", u"title3", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab3);
  // This tab will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab_received =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group.saved_guid());
  EXPECT_CALL(
      *notifier_observer_,
      OnTabAdded(TabGuidEq(tab3), Eq(tab_groups::TriggerSource::REMOTE)))
      .WillOnce(SaveArg<0>(&tab_received));
  UpdateTabGroup(tab_group, tab_groups::TriggerSource::REMOTE);

  // Remove a tab from the group and update it.
  GaiaId removed_by("user_id");
  tab_group.RemoveTabFromSync(tab1.saved_tab_guid(), removed_by);
  EXPECT_CALL(*notifier_observer_,
              OnTabRemoved(TabGuidEq(tab1),
                           Eq(tab_groups::TriggerSource::REMOTE), Eq(false)))
      .WillOnce(SaveArg<0>(&tab_received));
  UpdateTabGroup(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_received.shared_attribution().updated_by, removed_by);

  // Create an update of tab 2.
  tab_groups::SavedTabGroup updated_tab_group = tab_group;
  tab_groups::SavedTabGroupTab tab2_updated = tab2;
  tab2_updated.SetURL(GURL("https://www.example.com/subpage/"));
  updated_tab_group.UpdateTab(tab2_updated);
  // This tab will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab2_updated_received =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group.saved_guid());
  tab_groups::SavedTabGroupTab tab2_before_received =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group.saved_guid());
  EXPECT_CALL(*notifier_observer_,
              OnTabUpdated(TabGuidEq(tab2), TabGuidEq(tab2),
                           Eq(tab_groups::TriggerSource::REMOTE), Eq(false)))
      .WillOnce(DoAll(SaveArg<0>(&tab2_before_received),
                      SaveArg<1>(&tab2_updated_received)));

  UpdateTabGroup(updated_tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab2_updated.url(), tab2_updated_received.url());
  EXPECT_EQ(tab2.url(), tab2_before_received.url());

  // Verify that we have stored the updated tab by restoring tab 2 to its
  // original and it should be considered an updated.
  tab_groups::SavedTabGroup restored_tab_group = tab_group;
  tab_groups::SavedTabGroupTab tab2_restored = tab2_updated;
  tab2_restored.SetURL(GURL("https://www.example.com/"));
  updated_tab_group.UpdateTab(tab2_restored);
  // This tab will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab2_restored_received =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group.saved_guid());
  EXPECT_CALL(*notifier_observer_,
              OnTabUpdated(TabGuidEq(tab2), TabGuidEq(tab2_restored),
                           Eq(tab_groups::TriggerSource::REMOTE), Eq(false)))
      .WillOnce(SaveArg<1>(&tab2_restored_received));
  UpdateTabGroup(restored_tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab2_restored.url(), tab2_restored_received.url());
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabUpdatedBasedOnSpecificFields) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab = CreateSavedTabGroupTab(
      "https://www.example.com/", u"title", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab);

  // First add the group.
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded);
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);

  // Updating the URL should result in an update.
  tab_groups::SavedTabGroupTab tab_url_updated = tab;
  tab_url_updated.SetURL(GURL("https://www.example.com/subpage/"));
  tab_groups::SavedTabGroupTab updated_tab_received =
      GetUpdatedTab(tab_group, tab_url_updated);
  EXPECT_EQ(tab_url_updated.url(), updated_tab_received.url());

  // Updating the title should NOT result in an update.
  tab_groups::SavedTabGroupTab tab_title_updated = updated_tab_received;
  tab_title_updated.SetTitle(u"new title");
  VerifyTabNotUpdated(tab_group, tab_title_updated);
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabSelection) {
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  tab_groups::SavedTabGroup tab_group_1 = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("url", u"title", tab_group_1.saved_guid());
  tab_groups::SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("url", u"title", tab_group_1.saved_guid());
  tab_group_1.AddTabFromSync(tab1);
  tab_group_1.AddTabFromSync(tab2);

  // Set up notifier with initial tab group data.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_1),
                              Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabGroupAdded(tab_group_1,
                                  tab_groups::TriggerSource::REMOTE);
  testing::Mock::VerifyAndClearExpectations(tgss_observer_);

  // Select tab 1.
  {
    InSequence s;
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab1.local_tab_id().value()), Eq(true)))
        .Times(1);
    tgss_observer_->OnTabSelected(
        std::set<tab_groups::LocalTabID>({tab1.local_tab_id().value()}));
  }

  // Select tab 2. It will deselect tab 1.
  {
    InSequence s;
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab1.local_tab_id().value()), Eq(false)))
        .Times(1);
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab2.local_tab_id().value()), Eq(true)))
        .Times(1);
    tgss_observer_->OnTabSelected(
        std::set<tab_groups::LocalTabID>({tab2.local_tab_id().value()}));
  }

  // Select a tab outside tab groups. It will deselect the previous tab.
  tab_groups::LocalTabID outside_tab_id =
      tab_groups::test::GenerateRandomTabID();
  {
    InSequence s;
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab2.local_tab_id().value()), Eq(false)))
        .Times(1);
    EXPECT_CALL(*notifier_observer_,
                OnTabSelectionChanged(testing::Eq(outside_tab_id), Eq(true)))
        .Times(1);
    tgss_observer_->OnTabSelected(
        std::set<tab_groups::LocalTabID>({outside_tab_id}));
  }

  // Add another tab group.
  tab_groups::SavedTabGroup tab_group_2 = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab3 =
      CreateSavedTabGroupTab("url", u"title", tab_group_2.saved_guid());
  tab_group_2.AddTabFromSync(tab3);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_2),
                              Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabGroupAdded(tab_group_2,
                                  tab_groups::TriggerSource::REMOTE);

  // Select tab 3 from the new group.
  {
    InSequence s;
    EXPECT_CALL(*notifier_observer_,
                OnTabSelectionChanged(Eq(outside_tab_id), Eq(false)))
        .Times(1);
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab3.local_tab_id().value()), Eq(true)))
        .Times(1);
    tgss_observer_->OnTabSelected(
        std::set<tab_groups::LocalTabID>({tab3.local_tab_id().value()}));
  }

  // Select the first tab again.
  {
    InSequence s;
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab3.local_tab_id().value()), Eq(false)))
        .Times(1);
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab1.local_tab_id().value()), Eq(true)))
        .Times(1);
    tgss_observer_->OnTabSelected(
        std::set<tab_groups::LocalTabID>({tab1.local_tab_id().value()}));
  }

  // Select tab 2 again while keeping tab 1 selected.
  {
    InSequence s;
    EXPECT_CALL(
        *notifier_observer_,
        OnTabSelectionChanged(Eq(tab2.local_tab_id().value()), Eq(true)))
        .Times(1);
    tgss_observer_->OnTabSelected(std::set<tab_groups::LocalTabID>(
        {tab1.local_tab_id().value(), tab2.local_tab_id().value()}));
  }
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabLastSeenUpdated) {
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab =
      CreateSavedTabGroupTab("url", u"title", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab);

  // Test that the TabGroupChangeNotifier::Observer method is called
  // from the TabGroupSyncService::Observer, forwarding all parameters.
  EXPECT_CALL(*notifier_observer_,
              OnTabLastSeenTimeChanged(tab.saved_tab_guid(),
                                       Eq(tab_groups::TriggerSource::REMOTE)));
  tgss_observer_->OnTabLastSeenTimeChanged(tab.saved_tab_guid(),
                                           tab_groups::TriggerSource::REMOTE);
}

TEST_F(TabGroupChangeNotifierImplTest, OpenAndCloseTabGroup) {
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroup();
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(tab_group.saved_guid()))
      .WillRepeatedly(Return(tab_group));
  EXPECT_CALL(*notifier_observer_, OnTabGroupOpened(_));
  notifier_->OnTabGroupOpenedOrClosed(
      tab_group.saved_guid(), tab_groups::test::GenerateRandomTabGroupID());
  EXPECT_CALL(*notifier_observer_, OnTabGroupClosed(_));
  notifier_->OnTabGroupOpenedOrClosed(tab_group.saved_guid(), std::nullopt);
}

// Tests a case that a tab is removed by remote, but OnTabGroupUpdated is
// called with a LOCAL trigger source due to pending NTP tab.
TEST_F(TabGroupChangeNotifierImplTest,
       TestTabRemovedByRmoteButTabGroupUpdatedLocally) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab1 = CreateSavedTabGroupTab(
      "https://www.example.com/", u"title", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab1);

  // First add the group.
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded);
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);

  // Add a new tab to the group and update it.
  tab_groups::SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("url3", u"title2", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab2);
  // Remove a tab from the group and update it.
  GaiaId removed_by("user_id");
  tab_group.RemoveTabFromSync(tab1.saved_tab_guid(), removed_by);
  // This tab will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab_received =
      CreateSavedTabGroupTab("N/A", u"N/A", tab_group.saved_guid());
  EXPECT_CALL(*notifier_observer_,
              OnTabRemoved(TabGuidEq(tab1),
                           Eq(tab_groups::TriggerSource::LOCAL), Eq(false)))
      .WillOnce(SaveArg<0>(&tab_received));
  UpdateTabGroup(tab_group, tab_groups::TriggerSource::LOCAL);

  // The removed tab should have the correct user that removed it.
  EXPECT_EQ(tab_received.shared_attribution().updated_by, removed_by);
}

}  // namespace collaboration::messaging
