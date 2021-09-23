// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_factory.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::HasSubstr;
using testing::NiceMock;
using testing::Return;

namespace reporting {

class MockReportQueueConsumer {
 public:
  MockReportQueueConsumer() = default;
  void SetReportQueue(std::unique_ptr<ReportQueue> report_queue) {
    report_queue_ = std::move(report_queue);
  }
  base::OnceCallback<void(std::unique_ptr<reporting::ReportQueue>)>
  GetReportQueueSetter() {
    return base::BindOnce(&MockReportQueueConsumer::SetReportQueue,
                          weak_factory_.GetWeakPtr());
  }
  ReportQueue* GetReportQueue() const { return report_queue_.get(); }

 private:
  std::unique_ptr<ReportQueue> report_queue_;
  base::WeakPtrFactory<MockReportQueueConsumer> weak_factory_{this};
};

class ReportQueueFactoryTest : public ::testing::Test {
 protected:
  ReportQueueFactoryTest() = default;

  void SetUp() override {
    consumer_ = std::make_unique<MockReportQueueConsumer>();
    provider_ = std::make_unique<NiceMock<MockReportQueueProvider>>();
    report_queue_provider_test_helper::SetForTesting(provider_.get());
  }

  void TearDown() override {
    report_queue_provider_test_helper::SetForTesting(nullptr);
  }

  const std::string dm_token_ = "TOKEN";
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockReportQueueConsumer> consumer_;
  std::unique_ptr<MockReportQueueProvider> provider_;
};

TEST_F(ReportQueueFactoryTest, CreateAndGetQueue) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  reporting::ReportQueueFactory::Create(dm_token_, destination_,
                                        consumer_->GetReportQueueSetter());
  EXPECT_CALL(*provider_.get(), InitOnCompletedCalled()).Times(1);
  provider_->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  task_environment_.RunUntilIdle();
  // we expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, EmptyDmToken) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  reporting::ReportQueueFactory::Create("", destination_,
                                        consumer_->GetReportQueueSetter());
  EXPECT_CALL(*provider_.get(), InitOnCompletedCalled()).Times(1);
  provider_->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  task_environment_.RunUntilIdle();
  // we expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

// Tests if two consumers use the same provider and create two queues.
TEST_F(ReportQueueFactoryTest, SameProviderForMultipleThreads) {
  const std::string dmToken2 = "TOKEN2";
  auto consumer2 = std::make_unique<MockReportQueueConsumer>();
  EXPECT_FALSE(consumer_->GetReportQueue());
  EXPECT_FALSE(consumer2->GetReportQueue());
  reporting::ReportQueueFactory::Create(dm_token_, destination_,
                                        consumer_->GetReportQueueSetter());
  reporting::ReportQueueFactory::Create(dmToken2, destination_,
                                        consumer2->GetReportQueueSetter());
  EXPECT_CALL(*provider_.get(), InitOnCompletedCalled()).Times(1);
  provider_->ExpectCreateNewQueueAndReturnNewMockQueue(2);
  task_environment_.RunUntilIdle();
  // we expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
  // and for the 2nd consumer
  EXPECT_TRUE(consumer2->GetReportQueue());
}

}  // namespace reporting
