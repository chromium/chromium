// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/history_tracker.h"

#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/health.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;

namespace reporting {
namespace {

class HistoryTrackerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;

  const raw_ptr<HistoryTracker> tracker_{HistoryTracker::Get()};
};

TEST_F(HistoryTrackerTest, DebugStateTest) {
  // False by default.
  EXPECT_FALSE(tracker_->debug_state());

  // Enable and check.
  tracker_->set_debug_state(true);
  EXPECT_TRUE(tracker_->debug_state());

  // Disable and check
  tracker_->set_debug_state(false);
  EXPECT_FALSE(tracker_->debug_state());
}

TEST_F(HistoryTrackerTest, CollectHistoryTest) {
  ERPHealthData data;
  auto* const enqueue_record =
      data.add_history()->mutable_enqueue_record_call();
  enqueue_record->set_priority(Priority::FAST_BATCH);
  enqueue_record->set_destination(Destination::HEARTBEAT_EVENTS);
  auto* const upload_record =
      data.add_history()->mutable_upload_encrypted_record_call();
  upload_record->add_items()->mutable_record();
  upload_record->add_items()->mutable_gap();
  upload_record->set_upload_reason("PERIODIC");
  upload_record->set_priority(Priority::SLOW_BATCH);

  std::string expected_result;
  ASSERT_TRUE(data.SerializeToString(&expected_result));

  {
    test::TestCallbackAutoWaiter waiter;
    tracker_->set_data(std::move(data),
                       base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                      base::Unretained(&waiter)));
  }

  {
    test::TestEvent<const ERPHealthData&> received;
    tracker_->retrieve_data(received.cb());
    const auto& result = received.ref_result();
    std::string actual_result;
    ASSERT_TRUE(result.SerializeToString(&actual_result));
    EXPECT_THAT(actual_result, StrEq(expected_result));
  }
}

TEST_F(HistoryTrackerTest, ObserversTest) {
  class MockObserver : public HistoryTracker::Observer {
   public:
    MOCK_METHOD(void, OnNewData, (const ERPHealthData& data), (const override));
  };

  ::testing::StrictMock<MockObserver> observer1;
  ::testing::StrictMock<MockObserver> observer2;
  ::testing::StrictMock<MockObserver> observer3;

  tracker_->AddObserver(&observer1);

  EXPECT_CALL(observer1, OnNewData).Times(1);
  EXPECT_CALL(observer2, OnNewData).Times(0);
  EXPECT_CALL(observer3, OnNewData).Times(0);

  {
    ERPHealthData data;
    test::TestCallbackAutoWaiter waiter;
    tracker_->set_data(std::move(data),
                       base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                      base::Unretained(&waiter)));
  }

  tracker_->AddObserver(&observer2);
  tracker_->AddObserver(&observer3);

  EXPECT_CALL(observer1, OnNewData).Times(1);
  EXPECT_CALL(observer2, OnNewData).Times(1);
  EXPECT_CALL(observer3, OnNewData).Times(1);

  {
    ERPHealthData data;
    test::TestCallbackAutoWaiter waiter;
    tracker_->set_data(std::move(data),
                       base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                      base::Unretained(&waiter)));
  }

  tracker_->RemoveObserver(&observer2);

  EXPECT_CALL(observer1, OnNewData).Times(1);
  EXPECT_CALL(observer2, OnNewData).Times(0);
  EXPECT_CALL(observer3, OnNewData).Times(1);

  {
    ERPHealthData data;
    test::TestCallbackAutoWaiter waiter;
    tracker_->set_data(std::move(data),
                       base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                      base::Unretained(&waiter)));
  }

  tracker_->RemoveObserver(&observer1);
  tracker_->RemoveObserver(&observer3);

  EXPECT_CALL(observer1, OnNewData).Times(0);
  EXPECT_CALL(observer2, OnNewData).Times(0);
  EXPECT_CALL(observer3, OnNewData).Times(0);

  {
    ERPHealthData data;
    test::TestCallbackAutoWaiter waiter;
    tracker_->set_data(std::move(data),
                       base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                      base::Unretained(&waiter)));
  }

  task_environment_.RunUntilIdle();  // Drain possible unfinished tasks.
}
}  // namespace
}  // namespace reporting
