// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/messaging_backend_service_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
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

class MockMessagingBackendStore : public MessagingBackendStore {
 public:
  MockMessagingBackendStore() = default;
  ~MockMessagingBackendStore() override = default;

  MOCK_METHOD(void,
              Initialize,
              (base::OnceCallback<void(bool)> on_initialized_callback),
              (override));
  MOCK_METHOD(bool, HasAnyDirtyMessages, (DirtyType dirty_type), (override));
  MOCK_METHOD(void,
              ClearDirtyMessageForTab,
              (const data_sharing::GroupId& collaboration_id,
               const base::Uuid& tab_id,
               DirtyType dirty_type),
              (override));
  MOCK_METHOD(void,
              ClearDirtyMessage,
              (const base::Uuid uuid, DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::vector<collaboration_pb::Message>,
              GetDirtyMessages,
              (DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::vector<collaboration_pb::Message>,
              GetDirtyMessagesForGroup,
              (const data_sharing::GroupId& collaboration_id,
               DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::optional<collaboration_pb::Message>,
              GetDirtyMessageForTab,
              (const data_sharing::GroupId& collaboration_id,
               const base::Uuid& tab_id,
               DirtyType dirty_type),
              (override));
  MOCK_METHOD(std::vector<collaboration_pb::Message>,
              GetRecentMessagesForGroup,
              (const data_sharing::GroupId& collaboration_id),
              (override));
  MOCK_METHOD(void,
              AddMessage,
              (const collaboration_pb::Message& message),
              (override));
  MOCK_METHOD(base::TimeDelta, GetRecentMessageCutoffDuration, (), (override));
  MOCK_METHOD(void,
              SetRecentMessageCutoffDuration,
              (base::TimeDelta time_delta),
              (override));
};

class MockDataSharingChangeNotifier : public DataSharingChangeNotifier {
 public:
  MockDataSharingChangeNotifier() = default;
  ~MockDataSharingChangeNotifier() override = default;

  MOCK_METHOD(void,
              AddObserver,
              (DataSharingChangeNotifier::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (DataSharingChangeNotifier::Observer * observer),
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
        .WillOnce(SaveArg<0>(&tg_notifier_observer_));
    EXPECT_CALL(*unowned_tab_group_change_notifier_, Initialize());
    EXPECT_CALL(*unowned_tab_group_change_notifier_, RemoveObserver(_));

    auto data_sharing_change_notifier =
        std::make_unique<MockDataSharingChangeNotifier>();
    unowned_data_sharing_change_notifier_ = data_sharing_change_notifier.get();
    EXPECT_CALL(*unowned_data_sharing_change_notifier_, AddObserver(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&ds_notifier_observer_));
    EXPECT_CALL(*unowned_data_sharing_change_notifier_, Initialize());
    EXPECT_CALL(*unowned_data_sharing_change_notifier_, RemoveObserver(_));

    auto mock_messaging_backend_store =
        std::make_unique<MockMessagingBackendStore>();
    unowned_messaging_backend_store_ = mock_messaging_backend_store.get();

    service_ = std::make_unique<MessagingBackendServiceImpl>(
        std::move(tab_group_change_notifier),
        std::move(data_sharing_change_notifier),
        std::move(mock_messaging_backend_store),
        mock_tab_group_sync_service_.get(),
        /*data_sharing_service=*/nullptr);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  std::unique_ptr<MessagingBackendServiceImpl> service_;
  raw_ptr<MockTabGroupChangeNotifier> unowned_tab_group_change_notifier_;
  raw_ptr<MockDataSharingChangeNotifier> unowned_data_sharing_change_notifier_;
  raw_ptr<MockMessagingBackendStore> unowned_messaging_backend_store_;
  raw_ptr<TabGroupChangeNotifier::Observer> tg_notifier_observer_;
  raw_ptr<DataSharingChangeNotifier::Observer> ds_notifier_observer_;
};

TEST_F(MessagingBackendServiceImplTest, TestInitialization) {
  CreateService();
  EXPECT_FALSE(service_->IsInitialized());
  tg_notifier_observer_->OnTabGroupChangeNotifierInitialized();
  EXPECT_FALSE(service_->IsInitialized());
  ds_notifier_observer_->OnDataSharingChangeNotifierInitialized();
  EXPECT_TRUE(service_->IsInitialized());
}

}  // namespace collaboration::messaging
