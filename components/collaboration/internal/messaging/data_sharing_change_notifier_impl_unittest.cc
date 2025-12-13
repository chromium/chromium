// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace collaboration::messaging {

bool OptionalGroupMetadataDataMatches(
    std::optional<data_sharing::GroupData> a,
    std::optional<data_sharing::GroupData> b) {
  if (!a.has_value() && !b.has_value()) {
    return true;
  }
  if (a.has_value() != b.has_value()) {
    return false;
  }
  // Both have values, compare the GroupData.
  return a->group_token.group_id == b->group_token.group_id &&
         a->display_name == b->display_name;
}

MATCHER_P(OptionalGroupMetadataDataEq, expected_group_data, "") {
  return OptionalGroupMetadataDataMatches(expected_group_data, arg);
}

class MockDataSharingChangeNotifierObserver
    : public DataSharingChangeNotifier::Observer {
 public:
  MockDataSharingChangeNotifierObserver() = default;
  ~MockDataSharingChangeNotifierObserver() override = default;

  MOCK_METHOD(void, OnDataSharingChangeNotifierInitialized, (), (override));
  MOCK_METHOD(void,
              OnGroupAdded,
              (const data_sharing::GroupId& group_id,
               const std::optional<data_sharing::GroupData>& group_data,
               const base::Time& event_time),
              (override));
  MOCK_METHOD(void,
              OnGroupRemoved,
              (const data_sharing::GroupId& group_id,
               const std::optional<data_sharing::GroupData>& group_data,
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
  MOCK_METHOD(void,
              OnSyncBridgeUpdateTypeChanged,
              (data_sharing::SyncBridgeUpdateType));
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

  DataSharingChangeNotifier::FlushCallback InitializeNotifier(
      bool data_sharing_service_initialized) {
    EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
        .WillOnce(Return(data_sharing_service_initialized));
    EXPECT_CALL(*data_sharing_service_, AddObserver(_))
        .WillOnce(SaveArg<0>(&dss_observer_));
    return notifier_->Initialize();
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

TEST_F(DataSharingChangeNotifierImplTest, TestPublishOnlyHappensAfterFlush) {
  DataSharingChangeNotifier::FlushCallback flush_callback =
      InitializeNotifier(/*data_sharing_service_initialized=*/false);
  EXPECT_CALL(*notifier_observer_, OnDataSharingChangeNotifierInitialized());
  dss_observer_->OnGroupDataModelLoaded();

  data_sharing::GroupId group_id = data_sharing::GroupId("group_id");
  data_sharing::GroupData group = data_sharing::GroupData();
  group.group_token.group_id = group_id;
  GaiaId gaia_id = GaiaId("abc");

  // Observer calls should not happen for any of these yet.
  EXPECT_CALL(*notifier_observer_, OnGroupAdded(_, _, _)).Times(0);
  dss_observer_->OnGroupAdded(group, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupRemoved(_, _, _)).Times(0);
  dss_observer_->OnGroupRemoved(group_id, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberAdded(_, _, _)).Times(0);
  dss_observer_->OnGroupMemberAdded(group_id, GaiaId(), base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberRemoved(_, _, _)).Times(0);
  dss_observer_->OnGroupMemberRemoved(group_id, GaiaId(), base::Time());

  // Prepare for flushing.
  EXPECT_CALL(*data_sharing_service_, GetGroupEventsSinceStartup)
      .WillOnce(testing::Return(std::vector<data_sharing::GroupEvent>()));

  // Process any startup changes and make the notifier ready.
  std::move(flush_callback).Run();

  // Lookups need to work while publishing.
  EXPECT_CALL(*data_sharing_service_, ReadGroup(group_id))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*data_sharing_service_, GetPossiblyRemovedGroup(group_id))
      .WillRepeatedly(
          testing::Return(std::make_optional<data_sharing::GroupData>(group)));

  EXPECT_CALL(*notifier_observer_, OnGroupAdded(_, _, _)).Times(1);
  dss_observer_->OnGroupAdded(group, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupRemoved(_, _, _)).Times(1);
  dss_observer_->OnGroupRemoved(group_id, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberAdded(_, gaia_id, _)).Times(1);
  dss_observer_->OnGroupMemberAdded(group_id, gaia_id, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberRemoved(_, gaia_id, _))
      .Times(1);
  dss_observer_->OnGroupMemberRemoved(group_id, gaia_id, base::Time());
}

TEST_F(DataSharingChangeNotifierImplTest,
       TestFlushingPublishesCorrectGroupEvents) {
  DataSharingChangeNotifier::FlushCallback flush_callback =
      InitializeNotifier(/*data_sharing_service_initialized=*/false);
  EXPECT_CALL(*notifier_observer_, OnDataSharingChangeNotifierInitialized());
  dss_observer_->OnGroupDataModelLoaded();

  GaiaId gaia_id_1 = GaiaId("abc");
  GaiaId gaia_id_2 = GaiaId("def");

  data_sharing::GroupId group_id_1 = data_sharing::GroupId("group_id_1");
  data_sharing::GroupData group_1 = data_sharing::GroupData();
  group_1.group_token.group_id = group_id_1;
  group_1.display_name = "abc group";

  data_sharing::GroupId group_id_2 = data_sharing::GroupId("group_id_2");
  data_sharing::GroupData group_2 = data_sharing::GroupData();
  group_2.group_token.group_id = group_id_2;
  group_2.display_name = "def group";

  // Group 1 is available now, and Group 2 is available as a removed group.
  EXPECT_CALL(*data_sharing_service_, ReadGroup(group_id_1))
      .WillRepeatedly(testing::Return(group_1));
  EXPECT_CALL(*data_sharing_service_, ReadGroup(group_id_2))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*data_sharing_service_, GetPossiblyRemovedGroup(group_id_1))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*data_sharing_service_, GetPossiblyRemovedGroup(group_id_2))
      .WillRepeatedly(testing::Return(group_2));

  // We expect to get one of each of these API calls.
  EXPECT_CALL(*notifier_observer_,
              OnGroupAdded(group_id_1, OptionalGroupMetadataDataEq(group_1), _))
      .Times(1);
  EXPECT_CALL(
      *notifier_observer_,
      OnGroupRemoved(group_id_2, OptionalGroupMetadataDataEq(group_2), _))
      .Times(1);
  EXPECT_CALL(*notifier_observer_, OnGroupMemberAdded(_, gaia_id_1, _))
      .Times(1);
  EXPECT_CALL(*notifier_observer_, OnGroupMemberRemoved(_, gaia_id_2, _))
      .Times(1);

  // Prepare for flushing.
  std::vector<data_sharing::GroupEvent> group_events = {};
  group_events.emplace_back(data_sharing::GroupEvent::EventType::kGroupAdded,
                            group_id_1, std::nullopt, base::Time());
  group_events.emplace_back(data_sharing::GroupEvent::EventType::kGroupRemoved,
                            group_id_2, std::nullopt, base::Time());
  group_events.emplace_back(data_sharing::GroupEvent::EventType::kMemberAdded,
                            group_id_1, gaia_id_1, base::Time());
  group_events.emplace_back(data_sharing::GroupEvent::EventType::kMemberRemoved,
                            group_id_1, gaia_id_2, base::Time());
  EXPECT_CALL(*data_sharing_service_, GetGroupEventsSinceStartup)
      .WillOnce(testing::Return(group_events));

  // Process any startup changes and make the notifier ready.
  std::move(flush_callback).Run();
}

TEST_F(DataSharingChangeNotifierImplTest,
       TestPublishDoesnotHappenDuringInitialMerge) {
  DataSharingChangeNotifier::FlushCallback flush_callback =
      InitializeNotifier(/*data_sharing_service_initialized=*/false);
  EXPECT_CALL(*notifier_observer_, OnDataSharingChangeNotifierInitialized());
  dss_observer_->OnGroupDataModelLoaded();
  // Make the notifier ready.
  std::move(flush_callback).Run();

  data_sharing::GroupId group_id = data_sharing::GroupId("group_id");
  data_sharing::GroupData group = data_sharing::GroupData();
  group.group_token.group_id = group_id;
  GaiaId gaia_id = GaiaId("abc");

  // Lookups need to work while publishing.
  EXPECT_CALL(*data_sharing_service_, ReadGroup(group_id))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*data_sharing_service_, GetPossiblyRemovedGroup(group_id))
      .WillRepeatedly(
          testing::Return(std::make_optional<data_sharing::GroupData>(group)));

  // By default, observers should receive sync events.
  EXPECT_CALL(*notifier_observer_, OnGroupAdded(_, _, _)).Times(1);
  dss_observer_->OnGroupAdded(group, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupRemoved(_, _, _)).Times(1);
  dss_observer_->OnGroupRemoved(group_id, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberAdded(_, _, _)).Times(1);
  dss_observer_->OnGroupMemberAdded(group_id, GaiaId(), base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberRemoved(_, _, _)).Times(1);
  dss_observer_->OnGroupMemberRemoved(group_id, GaiaId(), base::Time());
  testing::Mock::VerifyAndClearExpectations(dss_observer_);

  // Mimic sign-in. Observers should not receive sync events during this time.
  dss_observer_->OnSyncBridgeUpdateTypeChanged(
      data_sharing::SyncBridgeUpdateType::kInitialMerge);
  EXPECT_CALL(*notifier_observer_, OnGroupAdded(_, _, _)).Times(0);
  dss_observer_->OnGroupAdded(group, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupRemoved(_, _, _)).Times(0);
  dss_observer_->OnGroupRemoved(group_id, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberAdded(_, _, _)).Times(0);
  dss_observer_->OnGroupMemberAdded(group_id, GaiaId(), base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberRemoved(_, _, _)).Times(0);
  dss_observer_->OnGroupMemberRemoved(group_id, GaiaId(), base::Time());
  testing::Mock::VerifyAndClearExpectations(dss_observer_);

  // Mimic signed-in state. Observers should receive sync events during this
  // time.
  dss_observer_->OnSyncBridgeUpdateTypeChanged(
      data_sharing::SyncBridgeUpdateType::kDefaultState);
  EXPECT_CALL(*notifier_observer_, OnGroupAdded(_, _, _)).Times(1);
  dss_observer_->OnGroupAdded(group, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupRemoved(_, _, _)).Times(1);
  dss_observer_->OnGroupRemoved(group_id, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberAdded(_, _, _)).Times(1);
  dss_observer_->OnGroupMemberAdded(group_id, GaiaId(), base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberRemoved(_, _, _)).Times(1);
  dss_observer_->OnGroupMemberRemoved(group_id, GaiaId(), base::Time());
  testing::Mock::VerifyAndClearExpectations(dss_observer_);

  // Mimic sign-out. Observers should not receive sync events during this time.
  dss_observer_->OnSyncBridgeUpdateTypeChanged(
      data_sharing::SyncBridgeUpdateType::kDisableSync);
  EXPECT_CALL(*notifier_observer_, OnGroupAdded(_, _, _)).Times(0);
  dss_observer_->OnGroupAdded(group, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupRemoved(_, _, _)).Times(0);
  dss_observer_->OnGroupRemoved(group_id, base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberAdded(_, _, _)).Times(0);
  dss_observer_->OnGroupMemberAdded(group_id, GaiaId(), base::Time());
  EXPECT_CALL(*notifier_observer_, OnGroupMemberRemoved(_, _, _)).Times(0);
  dss_observer_->OnGroupMemberRemoved(group_id, GaiaId(), base::Time());
}

}  // namespace collaboration::messaging
