// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/messaging_backend_service_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceClosure;

using testing::_;
using testing::A;
using testing::SaveArg;
using testing::Truly;

namespace collaboration::messaging {

class MockTabGroupChangeNotifier : public TabGroupChangeNotifier {
 public:
  MockTabGroupChangeNotifier() = default;
  ~MockTabGroupChangeNotifier() override = default;

  MOCK_METHOD(void,
              AddObserver,
              (TabGroupChangeNotifier::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (TabGroupChangeNotifier::Observer * observer),
              (override));
  MOCK_METHOD(void, Initialize, (), (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
};

class MessagingBackendServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    mock_tab_group_sync_service_ =
        std::make_unique<tab_groups::MockTabGroupSyncService>();
  }

  void TearDown() override {}

  void CreateService() {
    auto tab_group_change_notifier =
        std::make_unique<MockTabGroupChangeNotifier>();
    unowned_tab_group_change_notifier_ = tab_group_change_notifier.get();
    EXPECT_CALL(*unowned_tab_group_change_notifier_, AddObserver(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&notifier_observer_));
    EXPECT_CALL(*unowned_tab_group_change_notifier_, Initialize());
    EXPECT_CALL(*unowned_tab_group_change_notifier_, RemoveObserver(_));

    service_ = std::make_unique<MessagingBackendServiceImpl>(
        std::move(tab_group_change_notifier),
        mock_tab_group_sync_service_.get(),
        /*data_sharing_service=*/nullptr);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  std::unique_ptr<MessagingBackendServiceImpl> service_;
  raw_ptr<MockTabGroupChangeNotifier> unowned_tab_group_change_notifier_;
  raw_ptr<TabGroupChangeNotifier::Observer> notifier_observer_;
};

TEST_F(MessagingBackendServiceImplTest, TestInitialization) {
  CreateService();
  EXPECT_FALSE(service_->IsInitialized());
  notifier_observer_->OnTabGroupChangeNotifierInitialized();
  EXPECT_TRUE(service_->IsInitialized());
}

}  // namespace collaboration::messaging
