// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
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

tab_groups::SavedTabGroup CreateTestSharedTabGroup() {
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
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

  void TearDown() override {
    EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_)).Times(1);
    MaybeRemoveNotifierObserver();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

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
  tab_groups::SavedTabGroup tab_group_3 = CreateTestSharedTabGroup();

  // On startup, 2 tab groups are available.
  std::vector<tab_groups::SavedTabGroup> startup_tab_groups;
  startup_tab_groups.emplace_back(tab_group_1);
  startup_tab_groups.emplace_back(tab_group_2);

  // At initialization time, one of the initial tab groups has been removed, and
  // a new one has been added.
  std::vector<tab_groups::SavedTabGroup> init_tab_groups;
  init_tab_groups.emplace_back(tab_group_2);
  init_tab_groups.emplace_back(tab_group_3);

  // Ensure the observer is informed of both changes.
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupRemoved(TabGroupGuidEq(tab_group_1)));
  EXPECT_CALL(*notifier_observer_,
              OnTabGroupAdded(TabGroupGuidEq(tab_group_3)));

  InitializeNotifier(startup_tab_groups, init_tab_groups);
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

  // TODO(crbug.com/378421557): Handle updated groups and verify local adds
  //                            are stored for later comparisons.

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

}  // namespace collaboration::messaging
