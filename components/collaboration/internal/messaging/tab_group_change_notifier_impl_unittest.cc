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
#include "components/tab_groups/tab_group_color.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceClosure;

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace collaboration::messaging {

namespace {
bool TabGroupsHaveSameGuidAndType(tab_groups::SavedTabGroup a,
                                  tab_groups::SavedTabGroup b) {
  return a.is_shared_tab_group() == b.is_shared_tab_group() &&
         a.saved_guid() == b.saved_guid();
}

MATCHER_P(TabGroupGuidEq, expected_group, "") {
  return TabGroupsHaveSameGuidAndType(arg, expected_group);
}

bool TabsHaveSameGuid(std::optional<tab_groups::SavedTabGroupTab> a,
                      std::optional<tab_groups::SavedTabGroupTab> b) {
  if (!a || !b) {
    return false;
  }
  return (*a).saved_tab_guid() == (*b).saved_tab_guid();
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
  MOCK_METHOD(void, OnTabGroupAdded, (const tab_groups::SavedTabGroup&));
  MOCK_METHOD(void, OnTabGroupRemoved, (tab_groups::SavedTabGroup));
  MOCK_METHOD(void, OnTabGroupNameUpdated, (const tab_groups::SavedTabGroup&));
  MOCK_METHOD(void, OnTabGroupColorUpdated, (const tab_groups::SavedTabGroup&));
  MOCK_METHOD(void, OnTabAdded, (const tab_groups::SavedTabGroupTab&));
  MOCK_METHOD(void, OnTabRemoved, (tab_groups::SavedTabGroupTab));
  MOCK_METHOD(void, OnTabUpdated, (const tab_groups::SavedTabGroupTab&));
  MOCK_METHOD(void,
              OnTabSelected,
              (std::optional<tab_groups::SavedTabGroupTab>));
};

class TabGroupChangeNotifierImplTest : public testing::Test {
 public:
  TabGroupChangeNotifierImplTest() = default;
  ~TabGroupChangeNotifierImplTest() override = default;

  void SetUp() override {
    tab_group_sync_service_ =
        std::make_unique<tab_groups::MockTabGroupSyncService>();
    notifier_ = std::make_unique<TabGroupChangeNotifierImpl>(
        tab_group_sync_service_.get());
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
        tab_groups::test::CreateSavedTabGroupTab(
            "N/A", u"N/A", updated_tab_group.saved_guid());
    EXPECT_CALL(*notifier_observer_, OnTabUpdated(TabGuidEq(updated_tab)))
        .WillOnce(SaveArg<0>(&tab_update_received));
    tgss_observer_->OnTabGroupUpdated(updated_tab_group,
                                      tab_groups::TriggerSource::REMOTE);
    return tab_update_received;
  }

  void VerifyTabNotUpdated(const tab_groups::SavedTabGroup& old_group,
                           tab_groups::SavedTabGroupTab changed_tab) {
    tab_groups::SavedTabGroup updated_tab_group = old_group;
    updated_tab_group.UpdateTab(changed_tab);
    EXPECT_CALL(*notifier_observer_, OnTabUpdated(TabGuidEq(changed_tab)))
        .Times(0);
  }

  void TearDown() override {
    EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_)).Times(1);
    MaybeRemoveNotifierObserver();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

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
              OnTabGroupAdded(TabGroupGuidEq(tab_group_1)));
  tgss_observer_->OnTabGroupAdded(tab_group_1,
                                  tab_groups::TriggerSource::REMOTE);

  // Add another test group and ensure the observer is informed.
  tab_groups::SavedTabGroup tab_group_2 = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_2)));
  tgss_observer_->OnTabGroupAdded(tab_group_2,
                                  tab_groups::TriggerSource::REMOTE);

  // Remove the first group and ensure the observer is informed.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(tab_group_1)));
  tgss_observer_->OnTabGroupRemoved(tab_group_1.saved_guid(),
                                    tab_groups::TriggerSource::REMOTE);

  // Remove the second group and ensure the observer is informed.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(tab_group_2)));
  tgss_observer_->OnTabGroupRemoved(tab_group_2.saved_guid(),
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
              OnTabGroupRemoved(TabGroupGuidEq(tab_group_1)));
  // Capture the updated tab groups so we can verify we received the updates.
  // The created test groups will be overwritten from the SaveArg, but we don't
  // have an empty constructor.
  // `tab_group_2` has title change.
  tab_groups::SavedTabGroup tab_group_2_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_2_title_changed)))
      .WillOnce(SaveArg<0>(&tab_group_2_received));
  // `tab_group_3` has color change.
  tab_groups::SavedTabGroup tab_group_3_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_3_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_3_received));
  // `tab_group_4` has both title and color change.
  tab_groups::SavedTabGroup tab_group_4_received_title =
      CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_, OnTabGroupNameUpdated(TabGroupGuidEq(
                                       tab_group_4_title_and_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_4_received_title));
  tab_groups::SavedTabGroup tab_group_4_received_color =
      CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_, OnTabGroupColorUpdated(TabGroupGuidEq(
                                       tab_group_4_title_and_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_4_received_color));
  // New group.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_6)));

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

  // Local adds of groups should not be published.
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::LOCAL);
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded(_)).Times(0);

  // Verify that even if a tab group was added locally, we get informed about
  // remote title changes.
  tab_groups::SavedTabGroup tab_group_received = CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_title_changed = tab_group;
  tab_group_title_changed.SetTitle(tab_group.title() + u"_changed");
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_title_changed)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  tgss_observer_->OnTabGroupUpdated(tab_group_title_changed,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group_title_changed.title());
  EXPECT_EQ(tab_group_received.color(), tab_group_title_changed.color());

  // After a remote update of title that should be stored, we should still not
  // receive updates about a local color change.
  tab_groups::SavedTabGroup tab_group_color_changed = tab_group_title_changed;
  // Original color is `kBlue`.
  tab_group_color_changed.SetColor(tab_groups::TabGroupColorId::kOrange);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_color_changed)))
      .Times(0);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_color_changed)))
      .Times(0);
  tgss_observer_->OnTabGroupUpdated(tab_group_color_changed,
                                    tab_groups::TriggerSource::LOCAL);

  // Local deletes of groups should not be published.
  tgss_observer_->OnTabGroupRemoved(tab_group.saved_guid(),
                                    tab_groups::TriggerSource::LOCAL);
  EXPECT_CALL(*notifier_observer_, OnTabGroupRemoved(_)).Times(0);
}

TEST_F(TabGroupChangeNotifierImplTest, TestIgnoreSavedTabGroups) {
  tab_groups::SavedTabGroup saved_tab_group_1 =
      tab_groups::test::CreateTestSavedTabGroup();
  tab_groups::SavedTabGroup saved_tab_group_2 =
      tab_groups::test::CreateTestSavedTabGroup();
  tab_groups::SavedTabGroup shared_tab_group = CreateTestSharedTabGroup();

  // We should only be informed about the shared tab group after startup.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(shared_tab_group)))
      .Times(1);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(saved_tab_group_1)))
      .Times(0);
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(
          {saved_tab_group_1, shared_tab_group}));

  // Any saved tab groups added at runtime should be ignored.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(saved_tab_group_2)))
      .Times(0);
  tgss_observer_->OnTabGroupAdded(saved_tab_group_2,
                                  tab_groups::TriggerSource::REMOTE);

  // Verify title updates are not published.
  tab_groups::SavedTabGroup saved_tab_group_1_title_changed = saved_tab_group_1;
  saved_tab_group_1_title_changed.SetTitle(saved_tab_group_1.title() +
                                           u"_changed");
  tgss_observer_->OnTabGroupUpdated(saved_tab_group_1_title_changed,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupNameUpdated(TabGroupGuidEq(saved_tab_group_1_title_changed)))
      .Times(0);

  // Verify color updates are not published.
  tab_groups::SavedTabGroup saved_tab_group_2_color_changed = saved_tab_group_2;
  // Original coltitle mr is `kBlue`.
  saved_tab_group_2_color_changed.SetColor(
      tab_groups::TabGroupColorId::kOrange);
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupColorUpdated(TabGroupGuidEq(saved_tab_group_2_color_changed)))
      .Times(0);

  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(saved_tab_group_1)))
      .Times(0);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(saved_tab_group_2)))
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
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded(TabGroupGuidEq(tab_group)));
  tgss_observer_->OnTabGroupUpdated(tab_group,
                                    tab_groups::TriggerSource::REMOTE);
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
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded(_)).Times(3);

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
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_1_title_changed)))
      .WillOnce(SaveArg<0>(&tab_group_1_received));
  tgss_observer_->OnTabGroupUpdated(tab_group_1_title_changed,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_1_title_changed.title(), tab_group_1_received.title());

  // Verify color change.
  // This will be overridden by SaveArg.
  tab_groups::SavedTabGroup tab_group_2_received = CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_2_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_2_received));
  tgss_observer_->OnTabGroupUpdated(tab_group_2_color_changed,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_2_color_changed.color(), tab_group_2_received.color());

  // Verify title and color change.
  // These will be overridden by SaveArg.
  tab_groups::SavedTabGroup tab_group_3_received_name =
      CreateTestSharedTabGroup();
  tab_groups::SavedTabGroup tab_group_3_received_color =
      CreateTestSharedTabGroup();
  EXPECT_CALL(*notifier_observer_, OnTabGroupNameUpdated(TabGroupGuidEq(
                                       tab_group_3_title_and_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_3_received_name));
  EXPECT_CALL(*notifier_observer_, OnTabGroupColorUpdated(TabGroupGuidEq(
                                       tab_group_3_title_and_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_3_received_color));
  tgss_observer_->OnTabGroupUpdated(tab_group_3_title_and_color_changed,
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
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded(TabGroupGuidEq(tab_group)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group.title());
  EXPECT_EQ(tab_group_received.color(), tab_group.color());

  // Then update the title.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_title_changed)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  tgss_observer_->OnTabGroupUpdated(tab_group_title_changed,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group_title_changed.title());
  EXPECT_EQ(tab_group_received.color(), tab_group_title_changed.color());

  // Then update the color.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  tgss_observer_->OnTabGroupUpdated(tab_group_color_changed,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(), tab_group_color_changed.title());
  EXPECT_EQ(tab_group_received.color(), tab_group_color_changed.color());

  // Then update both title and color.
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupNameUpdated(TabGroupGuidEq(tab_group_title_and_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupColorUpdated(TabGroupGuidEq(tab_group_title_and_color_changed)))
      .WillOnce(SaveArg<0>(&tab_group_received));
  tgss_observer_->OnTabGroupUpdated(tab_group_title_and_color_changed,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab_group_received.title(),
            tab_group_title_and_color_changed.title());
  EXPECT_EQ(tab_group_received.color(),
            tab_group_title_and_color_changed.color());

  // Then remove the group.
  EXPECT_CALL(
      *notifier_observer_,
      OnTabGroupRemoved(TabGroupGuidEq(tab_group_title_and_color_changed)))
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
  tab_groups::SavedTabGroupTab tab1 = tab_groups::test::CreateSavedTabGroupTab(
      "url1", u"title1", tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab2 = tab_groups::test::CreateSavedTabGroupTab(
      "https://example.com/", u"title2", tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab3 = tab_groups::test::CreateSavedTabGroupTab(
      "url3", u"title3", tab_group_startup.saved_guid());
  tab_group_startup.AddTabFromSync(tab1);
  tab_group_startup.AddTabFromSync(tab2);
  tab_group_startup.AddTabFromSync(tab3);

  // At init tab4 was added, tab2 was updated, and tab1 was removed.
  tab_groups::SavedTabGroup tab_group_init = tab_group_startup;
  tab_group_init.RemoveTabFromSync(tab1.saved_tab_guid());
  tab_groups::SavedTabGroupTab tab2_updated = tab2;
  tab2_updated.SetURL(GURL("https://example.com/subpage/"));
  tab_group_init.UpdateTab(tab2_updated);
  tab_groups::SavedTabGroupTab tab4 = tab_groups::test::CreateSavedTabGroupTab(
      "url4", u"title4", tab_group_init.saved_guid());
  tab_group_init.AddTabFromSync(tab4);

  // These tabs will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab_received_added =
      tab_groups::test::CreateSavedTabGroupTab("N/A", u"N/A",
                                               tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab_received_updated =
      tab_groups::test::CreateSavedTabGroupTab("N/A", u"N/A",
                                               tab_group_startup.saved_guid());
  tab_groups::SavedTabGroupTab tab_received_removed =
      tab_groups::test::CreateSavedTabGroupTab("N/A", u"N/A",
                                               tab_group_startup.saved_guid());
  EXPECT_CALL(*notifier_observer_, OnTabAdded(TabGuidEq(tab4)))
      .WillOnce(SaveArg<0>(&tab_received_added));
  EXPECT_CALL(*notifier_observer_, OnTabUpdated(TabGuidEq(tab2_updated)))
      .WillOnce(SaveArg<0>(&tab_received_updated));
  EXPECT_CALL(*notifier_observer_, OnTabRemoved(TabGuidEq(tab1)))
      .WillOnce(SaveArg<0>(&tab_received_removed));

  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(
          {tab_group_startup}),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(
          {tab_group_init}));

  EXPECT_EQ(tab2_updated.url(), tab_received_updated.url());

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
  tab_groups::SavedTabGroupTab tab1 = tab_groups::test::CreateSavedTabGroupTab(
      "url1", u"title1", tab_group.saved_guid());
  tab_groups::SavedTabGroupTab tab2 = tab_groups::test::CreateSavedTabGroupTab(
      "https://www.example.com/", u"title2", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab1);
  tab_group.AddTabFromSync(tab2);

  // First add the group.
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded(_));
  tgss_observer_->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);

  // Add a new tab to the group and update it.
  tab_groups::SavedTabGroupTab tab3 = tab_groups::test::CreateSavedTabGroupTab(
      "url3", u"title3", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab3);
  // This tab will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab_received =
      tab_groups::test::CreateSavedTabGroupTab("N/A", u"N/A",
                                               tab_group.saved_guid());
  EXPECT_CALL(*notifier_observer_, OnTabAdded(TabGuidEq(tab3)))
      .WillOnce(SaveArg<0>(&tab_received));
  tgss_observer_->OnTabGroupUpdated(tab_group,
                                    tab_groups::TriggerSource::REMOTE);

  // Remove a tab from the group and update it.
  tab_group.RemoveTabFromSync(tab1.saved_tab_guid());
  EXPECT_CALL(*notifier_observer_, OnTabRemoved(TabGuidEq(tab1)))
      .WillOnce(SaveArg<0>(&tab_received));
  tgss_observer_->OnTabGroupUpdated(tab_group,
                                    tab_groups::TriggerSource::REMOTE);

  // Create an update of tab 2.
  tab_groups::SavedTabGroup updated_tab_group = tab_group;
  tab_groups::SavedTabGroupTab tab2_updated = tab2;
  tab2_updated.SetURL(GURL("https://www.example.com/subpage/"));
  updated_tab_group.UpdateTab(tab2_updated);
  // This tab will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab2_updated_received =
      tab_groups::test::CreateSavedTabGroupTab("N/A", u"N/A",
                                               tab_group.saved_guid());
  EXPECT_CALL(*notifier_observer_, OnTabUpdated(TabGuidEq(tab2_updated)))
      .WillOnce(SaveArg<0>(&tab2_updated_received));
  tgss_observer_->OnTabGroupUpdated(updated_tab_group,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab2_updated.url(), tab2_updated_received.url());

  // Verify that we have stored the updated tab by restoring tab 2 to its
  // original and it should be considered an updated.
  tab_groups::SavedTabGroup restored_tab_group = tab_group;
  tab_groups::SavedTabGroupTab tab2_restored = tab2_updated;
  tab2_restored.SetURL(GURL("https://www.example.com/"));
  updated_tab_group.UpdateTab(tab2_restored);
  // This tab will be overridden by the callback.
  tab_groups::SavedTabGroupTab tab2_restored_received =
      tab_groups::test::CreateSavedTabGroupTab("N/A", u"N/A",
                                               tab_group.saved_guid());
  EXPECT_CALL(*notifier_observer_, OnTabUpdated(TabGuidEq(tab2_restored)))
      .WillOnce(SaveArg<0>(&tab2_restored_received));
  tgss_observer_->OnTabGroupUpdated(restored_tab_group,
                                    tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(tab2_restored.url(), tab2_restored_received.url());
}

TEST_F(TabGroupChangeNotifierImplTest, TestTabUpdatedBasedOnSpecificFields) {
  // Initialize the notifier with an empty set of tab groups available on
  // startup and on init.
  InitializeNotifier(
      /*startup_tab_groups=*/std::vector<tab_groups::SavedTabGroup>(),
      /*init_tab_groups=*/std::vector<tab_groups::SavedTabGroup>());

  tab_groups::SavedTabGroup tab_group = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab = tab_groups::test::CreateSavedTabGroupTab(
      "https://www.example.com/", u"title", tab_group.saved_guid());
  tab_group.AddTabFromSync(tab);

  // First add the group.
  EXPECT_CALL(*notifier_observer_, OnTabGroupAdded(_));
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
  tab_groups::SavedTabGroupTab tab1 = tab_groups::test::CreateSavedTabGroupTab(
      "url", u"title", tab_group_1.saved_guid());
  tab_groups::SavedTabGroupTab tab2 = tab_groups::test::CreateSavedTabGroupTab(
      "url", u"title", tab_group_1.saved_guid());
  tab_group_1.AddTabFromSync(tab1);
  tab_group_1.AddTabFromSync(tab2);

  // Set up notifier with initial tab group data.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_1)));
  tgss_observer_->OnTabGroupAdded(tab_group_1,
                                  tab_groups::TriggerSource::REMOTE);

  // Select tab 1.
  EXPECT_CALL(*notifier_observer_, OnTabSelected(TabGuidEq(tab1))).Times(1);
  tgss_observer_->OnTabSelected(tab_group_1.saved_guid(),
                                tab1.saved_tab_guid());

  // Select tab 2.
  EXPECT_CALL(*notifier_observer_, OnTabSelected(TabGuidEq(tab2))).Times(1);
  tgss_observer_->OnTabSelected(tab_group_1.saved_guid(),
                                tab2.saved_tab_guid());

  // Select a tab outside tab groups.
  EXPECT_CALL(*notifier_observer_, OnTabSelected(testing::Eq(std::nullopt)))
      .Times(1);
  tgss_observer_->OnTabSelected(base::Uuid::GenerateRandomV4(),
                                base::Uuid::GenerateRandomV4());

  // Add another tab group.
  tab_groups::SavedTabGroup tab_group_2 = CreateTestSharedTabGroupWithNoTabs();
  tab_groups::SavedTabGroupTab tab3 = tab_groups::test::CreateSavedTabGroupTab(
      "url", u"title", tab_group_2.saved_guid());
  tab_group_2.AddTabFromSync(tab3);
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_2)));
  tgss_observer_->OnTabGroupAdded(tab_group_2,
                                  tab_groups::TriggerSource::REMOTE);

  // Select tab 3 from the new group.
  EXPECT_CALL(*notifier_observer_, OnTabSelected(TabGuidEq(tab3))).Times(1);
  tgss_observer_->OnTabSelected(tab_group_2.saved_guid(),
                                tab3.saved_tab_guid());

  // Select the first tab again.
  EXPECT_CALL(*notifier_observer_, OnTabSelected(TabGuidEq(tab1))).Times(1);
  tgss_observer_->OnTabSelected(tab_group_1.saved_guid(),
                                tab1.saved_tab_guid());
}

}  // namespace collaboration::messaging
