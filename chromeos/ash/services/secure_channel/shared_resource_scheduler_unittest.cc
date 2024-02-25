// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/shared_resource_scheduler.h"

#include <memory>
#include <optional>

#include "base/test/gtest_util.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

class SecureChannelSharedResourceSchedulerTest : public testing::Test {
 public:
  SecureChannelSharedResourceSchedulerTest(
      const SecureChannelSharedResourceSchedulerTest&) = delete;
  SecureChannelSharedResourceSchedulerTest& operator=(
      const SecureChannelSharedResourceSchedulerTest&) = delete;

 protected:
  SecureChannelSharedResourceSchedulerTest() = default;
  ~SecureChannelSharedResourceSchedulerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scheduler_ = std::make_unique<SharedResourceScheduler>();
  }

  void TearDown() override {
    // Each test empties the scheduler of all scheduled tasks.
    EXPECT_FALSE(scheduler_->GetNextScheduledRequest());
    EXPECT_FALSE(scheduler_->GetHighestPriorityOfScheduledRequests());
    EXPECT_TRUE(scheduler_->empty());
  }

  SharedResourceScheduler* scheduler() { return scheduler_.get(); }

 private:
  std::unique_ptr<SharedResourceScheduler> scheduler_;
};

TEST_F(SecureChannelSharedResourceSchedulerTest, OneRequest) {
  DeviceIdPair pair("remoteId", "localId");

  // Low priority.
  scheduler()->ScheduleRequest(pair, ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());
  auto next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
  EXPECT_FALSE(scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_TRUE(scheduler()->empty());

  // Medium priority.
  scheduler()->ScheduleRequest(pair, ConnectionPriority::kMedium);
  EXPECT_EQ(ConnectionPriority::kMedium,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kMedium, next_scheduled_request->second);
  EXPECT_FALSE(scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_TRUE(scheduler()->empty());

  // High priority.
  scheduler()->ScheduleRequest(pair, ConnectionPriority::kHigh);
  EXPECT_EQ(ConnectionPriority::kHigh,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kHigh, next_scheduled_request->second);
  EXPECT_FALSE(scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_TRUE(scheduler()->empty());

  // Schedule, then remove the request. Nothing should be returned when
  // GetNextScheduledRequest() is called.
  scheduler()->ScheduleRequest(pair, ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());
  scheduler()->RemoveScheduledRequest(pair);
  EXPECT_FALSE(scheduler()->GetNextScheduledRequest());
  EXPECT_FALSE(scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_TRUE(scheduler()->empty());

  // Add as low-priority, update to medium-priority.
  scheduler()->ScheduleRequest(pair, ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());
  scheduler()->UpdateRequestPriority(pair, ConnectionPriority::kMedium);
  EXPECT_EQ(ConnectionPriority::kMedium,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kMedium, next_scheduled_request->second);
}

TEST_F(SecureChannelSharedResourceSchedulerTest, MultipleRequests_OnePriority) {
  DeviceIdPair pair_1("remoteId1", "localId1");
  DeviceIdPair pair_2("remoteId2", "localId2");
  DeviceIdPair pair_3("remoteId3", "localId3");
  DeviceIdPair pair_4("remoteId4", "localId4");

  scheduler()->ScheduleRequest(pair_1, ConnectionPriority::kLow);
  scheduler()->ScheduleRequest(pair_2, ConnectionPriority::kLow);
  scheduler()->ScheduleRequest(pair_3, ConnectionPriority::kLow);
  scheduler()->ScheduleRequest(pair_4, ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // The requests should come out of the scheduler in the same order they were
  // added, since they are all the same priority.
  auto next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_1, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_2, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_3, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_4, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
}

TEST_F(SecureChannelSharedResourceSchedulerTest,
       MultipleRequests_DifferentPriorities) {
  DeviceIdPair pair_1("remoteId1", "localId1");
  DeviceIdPair pair_2("remoteId2", "localId2");
  DeviceIdPair pair_3("remoteId3", "localId3");

  // Add lower priorities first.
  scheduler()->ScheduleRequest(pair_1, ConnectionPriority::kLow);
  scheduler()->ScheduleRequest(pair_2, ConnectionPriority::kMedium);
  scheduler()->ScheduleRequest(pair_3, ConnectionPriority::kHigh);
  EXPECT_EQ(ConnectionPriority::kHigh,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Even though the high-priority request was added last, it should still be
  // the first to come out of the scheduler.
  auto next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_3, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kHigh, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kMedium,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Then the medium-priority request.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_2, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kMedium, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Last, the low-priority request.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_1, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
}

TEST_F(SecureChannelSharedResourceSchedulerTest,
       DifferentPriorities_MultipleRequestsPerPriority) {
  DeviceIdPair pair_1("remoteId1", "localId1");
  DeviceIdPair pair_2("remoteId2", "localId2");
  DeviceIdPair pair_3("remoteId3", "localId3");
  DeviceIdPair pair_4("remoteId4", "localId4");
  DeviceIdPair pair_5("remoteId5", "localId5");
  DeviceIdPair pair_6("remoteId6", "localId6");
  DeviceIdPair pair_7("remoteId7", "localId7");
  DeviceIdPair pair_8("remoteId8", "localId8");
  DeviceIdPair pair_9("remoteId9", "localId9");

  scheduler()->ScheduleRequest(pair_1, ConnectionPriority::kLow);
  scheduler()->ScheduleRequest(pair_2, ConnectionPriority::kMedium);
  scheduler()->ScheduleRequest(pair_3, ConnectionPriority::kHigh);
  scheduler()->ScheduleRequest(pair_4, ConnectionPriority::kLow);
  scheduler()->ScheduleRequest(pair_5, ConnectionPriority::kMedium);
  scheduler()->ScheduleRequest(pair_6, ConnectionPriority::kHigh);
  scheduler()->ScheduleRequest(pair_7, ConnectionPriority::kLow);
  scheduler()->ScheduleRequest(pair_8, ConnectionPriority::kMedium);
  scheduler()->ScheduleRequest(pair_9, ConnectionPriority::kHigh);
  EXPECT_EQ(ConnectionPriority::kHigh,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // First high-priority request first.
  auto next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_3, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kHigh, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kHigh,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Then, next high-priority request.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_6, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kHigh, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kHigh,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Update pair_9 to be low-priority.
  scheduler()->UpdateRequestPriority(pair_9, ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kMedium,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // The first medium-priority request should be next.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_2, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kMedium, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kMedium,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Update pair_5 to be low-priority.
  scheduler()->UpdateRequestPriority(pair_5, ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kMedium,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Update pair_4 to be high-priority. It should be next out of the scheduler.
  scheduler()->UpdateRequestPriority(pair_4, ConnectionPriority::kHigh);
  EXPECT_EQ(ConnectionPriority::kHigh,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_4, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kHigh, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kMedium,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // pair_8 is the last medium-priority request.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_8, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kMedium, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // The first low-priority request should be next.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_1, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Then, next low-priority request.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_7, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Then, next low-priority request, which was updated to low midway.
  next_scheduled_request = scheduler()->GetNextScheduledRequest();
  EXPECT_EQ(pair_9, next_scheduled_request->first);
  EXPECT_EQ(ConnectionPriority::kLow, next_scheduled_request->second);
  EXPECT_EQ(ConnectionPriority::kLow,
            *scheduler()->GetHighestPriorityOfScheduledRequests());
  EXPECT_FALSE(scheduler()->empty());

  // Remove the final remaining request.
  scheduler()->RemoveScheduledRequest(pair_5);
}

TEST_F(SecureChannelSharedResourceSchedulerTest, EdgeCases) {
  DeviceIdPair pair("remoteId", "localId");

  // Cannot update item priority before scheduling it.
  EXPECT_DCHECK_DEATH(
      scheduler()->UpdateRequestPriority(pair, ConnectionPriority::kLow));

  // Cannot remove item before scheduling it.
  EXPECT_DCHECK_DEATH(scheduler()->RemoveScheduledRequest(pair));
}

}  // namespace ash::secure_channel
