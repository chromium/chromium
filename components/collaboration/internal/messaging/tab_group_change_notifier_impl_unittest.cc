// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceClosure;

using testing::_;
using testing::SaveArg;

namespace collaboration::messaging {

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

  void InitializeNotifier() {
    EXPECT_CALL(*tab_group_sync_service_, AddObserver(_))
        .Times(1)
        .WillOnce([&](tab_groups::TabGroupSyncService::Observer* observer) {
          tgss_observer_ = observer;
          // The TabGroupSyncService currently uses re-entrancy when informing
          // its observers about its initialization state. We are mimicking that
          // behavior here.
          observer->OnInitialized();
        });
    notifier_->Initialize();
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
  base::RunLoop run_loop;
  EXPECT_CALL(*notifier_observer_, OnTabGroupChangeNotifierInitialized())
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  InitializeNotifier();
  run_loop.Run();
  EXPECT_EQ(true, notifier_->IsInitialized());
}

}  // namespace collaboration::messaging
