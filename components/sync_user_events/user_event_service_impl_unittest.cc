// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_service_impl.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_user_events/user_event_sync_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

namespace {

std::unique_ptr<UserEventSpecifics> Event() {
  return std::make_unique<UserEventSpecifics>();
}

std::unique_ptr<UserEventSpecifics> AsTest(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_test_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> AsGaiaPasswordReuseEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_gaia_password_reuse_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> AsGaiaPasswordCaptured(
    std::unique_ptr<UserEventSpecifics> specifics) {
  specifics->mutable_gaia_password_captured_event();
  return specifics;
}

std::unique_ptr<UserEventSpecifics> WithNav(
    std::unique_ptr<UserEventSpecifics> specifics,
    int64_t navigation_id = 1) {
  specifics->set_navigation_id(navigation_id);
  return specifics;
}

class TestGlobalIdMapper : public GlobalIdMapper {
  void AddGlobalIdChangeObserver(GlobalIdChange callback) override {}
  int64_t GetLatestGlobalId(int64_t global_id) override { return global_id; }
};

class UserEventServiceImplTest : public testing::Test {
 protected:
  UserEventServiceImplTest() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kHistory});
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_processor_, TrackedAccountId())
        .WillByDefault(testing::Return("account_id"));
  }

  std::unique_ptr<UserEventSyncBridge> MakeBridge() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_processor_, ModelReadyToSync).WillOnce([&run_loop]() {
      run_loop.Quit();
    });
    auto bridge = std::make_unique<UserEventSyncBridge>(
        DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        mock_processor_.CreateForwardingProcessor(), &mapper_);
    run_loop.Run();
    return bridge;
  }

  syncer::TestSyncService* sync_service() { return &sync_service_; }
  MockDataTypeLocalChangeProcessor* mock_processor() {
    return &mock_processor_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  testing::NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  TestGlobalIdMapper mapper_;
};

TEST_F(UserEventServiceImplTest, ShouldRecord) {
  UserEventServiceImpl service(MakeBridge());
  EXPECT_CALL(*mock_processor(), Put);
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordWhenSyncIsNotStarted) {
  ON_CALL(*mock_processor(), IsTrackingMetadata())
      .WillByDefault(testing::Return(false));
  UserEventServiceImpl service(MakeBridge());

  // Do not record events when the engine is off.
  EXPECT_CALL(*mock_processor(), Put).Times(0);
  service.RecordUserEvent(WithNav(AsTest(Event())));
  service.RecordUserEvent(AsTest(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldNotRecordEmptyEvents) {
  UserEventServiceImpl service(MakeBridge());

  // All untyped events should always be ignored.
  EXPECT_CALL(*mock_processor(), Put).Times(0);
  service.RecordUserEvent(Event());
  service.RecordUserEvent(WithNav(Event()));
}

TEST_F(UserEventServiceImplTest, ShouldRecordHasNavigationId) {
  UserEventServiceImpl service(MakeBridge());

  // Verify logic for types that might or might not have a navigation id.
  EXPECT_CALL(*mock_processor(), Put);
  service.RecordUserEvent(AsTest(Event()));
  EXPECT_CALL(*mock_processor(), Put);
  service.RecordUserEvent(WithNav(AsTest(Event())));

  // Verify logic for types that must have a navigation id.
  EXPECT_CALL(*mock_processor(), Put).Times(0);
  service.RecordUserEvent(AsGaiaPasswordReuseEvent(Event()));
  EXPECT_CALL(*mock_processor(), Put);
  service.RecordUserEvent(WithNav(AsGaiaPasswordReuseEvent(Event())));

  // Verify logic for types that cannot have a navigation id.
  EXPECT_CALL(*mock_processor(), Put);
  service.RecordUserEvent(AsGaiaPasswordCaptured(Event()));
  EXPECT_CALL(*mock_processor(), Put).Times(0);
  service.RecordUserEvent(WithNav(AsGaiaPasswordCaptured(Event())));
}

TEST_F(UserEventServiceImplTest, SessionIdIsDifferent) {
  std::vector<int64_t> put_session_ids;
  ON_CALL(*mock_processor(), Put)
      .WillByDefault([&](const std::string& storage_key,
                         const std::unique_ptr<EntityData> entity_data,
                         MetadataChangeList* metadata_change_list) {
        put_session_ids.push_back(
            entity_data->specifics.user_event().session_id());
      });

  UserEventServiceImpl service1(MakeBridge());
  service1.RecordUserEvent(AsTest(Event()));

  UserEventServiceImpl service2(MakeBridge());
  service2.RecordUserEvent(AsTest(Event()));

  ASSERT_EQ(2U, put_session_ids.size());
  EXPECT_NE(put_session_ids[0], put_session_ids[1]);
}

}  // namespace

}  // namespace syncer
