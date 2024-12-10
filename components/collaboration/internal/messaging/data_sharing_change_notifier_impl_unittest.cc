// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace collaboration::messaging {

class MockDataSharingChangeNotifierObserver
    : public DataSharingChangeNotifier::Observer {
 public:
  MockDataSharingChangeNotifierObserver() = default;
  ~MockDataSharingChangeNotifierObserver() override = default;

  MOCK_METHOD(void, OnDataSharingChangeNotifierInitialized, (), (override));
  MOCK_METHOD(void,
              OnGroupAdded,
              (const data_sharing::GroupData& group_data,
               const base::Time& event_time),
              (override));
  MOCK_METHOD(void,
              OnGroupRemoved,
              (const data_sharing::GroupData& group_id,
               const base::Time& event_time),
              (override));
  MOCK_METHOD(void,
              OnGroupMemberAdded,
              (const data_sharing::GroupData& group_id,
               const GaiaId& member_gaia_id,
               const base::Time& event_time),
              (override));
  MOCK_METHOD(void,
              OnGroupMemberRemoved,
              (const data_sharing::GroupData& group_id,
               const GaiaId& member_gaia_id,
               const base::Time& event_time),
              (override));
};

class DataSharingChangeNotifierImplTest : public testing::Test {
 public:
  DataSharingChangeNotifierImplTest() = default;
  ~DataSharingChangeNotifierImplTest() override = default;

  void SetUp() override {
    data_sharing_service_ =
        std::make_unique<data_sharing::MockDataSharingService>();
    notifier_ = std::make_unique<DataSharingChangeNotifierImpl>(
        data_sharing_service_.get());
    notifier_observer_ =
        std::make_unique<MockDataSharingChangeNotifierObserver>();
    notifier_->AddObserver(notifier_observer_.get());
  }

  void InitializeNotifier(bool data_sharing_service_initialized) {
    EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
        .WillOnce(Return(data_sharing_service_initialized));
    EXPECT_CALL(*data_sharing_service_, AddObserver(_))
        .WillOnce(SaveArg<0>(&dss_observer_));
    notifier_->Initialize();
  }

  void TearDown() override {
    EXPECT_CALL(*data_sharing_service_, RemoveObserver(_)).Times(1);
    notifier_->RemoveObserver(notifier_observer_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<data_sharing::MockDataSharingService> data_sharing_service_;
  std::unique_ptr<DataSharingChangeNotifierImpl> notifier_;
  std::unique_ptr<MockDataSharingChangeNotifierObserver> notifier_observer_;

  raw_ptr<data_sharing::DataSharingService::Observer> dss_observer_;
};

TEST_F(DataSharingChangeNotifierImplTest,
       TestInitializationServiceAlreadyInitialized) {
  base::RunLoop run_loop;
  EXPECT_CALL(*notifier_observer_, OnDataSharingChangeNotifierInitialized())
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  InitializeNotifier(/*data_sharing_service_initialized=*/true);
  run_loop.Run();
  EXPECT_EQ(true, notifier_->IsInitialized());
}

TEST_F(DataSharingChangeNotifierImplTest,
       TestInitializationServiceInitializedLater) {
  InitializeNotifier(/*data_sharing_service_initialized=*/false);
  EXPECT_EQ(false, notifier_->IsInitialized());

  EXPECT_CALL(*notifier_observer_, OnDataSharingChangeNotifierInitialized());
  dss_observer_->OnGroupDataModelLoaded();
  EXPECT_EQ(true, notifier_->IsInitialized());
}

}  // namespace collaboration::messaging
