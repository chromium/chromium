// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider_test_helper.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

namespace reporting {

class MockRateLimiter : public RateLimiterInterface {
 public:
  MOCK_METHOD(bool, Acquire, (size_t event_size), (override));
};

class MockReportQueueConsumer {
 public:
  MockReportQueueConsumer() = default;
  void SetReportQueue(test::TestCallbackWaiter* waiter,
                      std::unique_ptr<ReportQueue> report_queue) {
    report_queue_ = std::move(report_queue);
    if (waiter) {
      waiter->Signal();
    }
  }
  base::OnceCallback<void(std::unique_ptr<ReportQueue>)> GetReportQueueSetter(
      test::TestCallbackWaiter* waiter) {
    return base::BindOnce(&MockReportQueueConsumer::SetReportQueue,
                          weak_factory_.GetWeakPtr(), base::Unretained(waiter));
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
    helper_ = std::make_unique<test::ReportQueueProviderTestHelper>();
  }

  void TearDown() override { helper_.reset(); }

  const Destination destination_ = Destination::UPLOAD_EVENTS;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockReportQueueConsumer> consumer_;
  std::unique_ptr<test::ReportQueueProviderTestHelper> helper_;
};

TEST_F(ReportQueueFactoryTest, CreateAndGetQueue) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    ReportQueueFactory::Create(
        ReportQueueConfiguration::Create(
            {.event_type = EventType::kDevice, .destination = destination_}),
        consumer_->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(1);
    helper_->mock_provider()->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateQueueWithInvalidConfig) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  ReportQueueFactory::Create(
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kDevice,
           .destination = Destination::UNDEFINED_DESTINATION}),
      consumer_->GetReportQueueSetter(nullptr));
  // Expect failure before it gets to the report queue provider
  EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(0);
  // We do not expect the report queue to be existing in the consumer.
  EXPECT_FALSE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateAndGetQueueWithRateLimiter) {
  EXPECT_FALSE(consumer_->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    ReportQueueFactory::Create(
        ReportQueueConfiguration::Create(
            {.event_type = EventType::kDevice, .destination = destination_})
            .SetRateLimiter(std::make_unique<MockRateLimiter>()),
        consumer_->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(1);
    helper_->mock_provider()->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateAndGetQueueWithValidReservedSpace) {
  EXPECT_FALSE(consumer_->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    ReportQueueFactory::Create(
        ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                          .destination = destination_,
                                          .reserved_space = 12345L}),
        consumer_->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(1);
    helper_->mock_provider()->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateQueueWithInvalidReservedSpace) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  ReportQueueFactory::Create(
      ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                        .destination = destination_,
                                        .reserved_space = -1L}),
      consumer_->GetReportQueueSetter(nullptr));
  // Expect failure before it gets to the report queue provider
  EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(0);
  // We do not expect the report queue to be existing in the consumer.
  EXPECT_FALSE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateAndGetQueueWithSource) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    SourceInfo source_info;
    source_info.set_source(SourceInfo::ASH);
    ReportQueueFactory::Create(
        ReportQueueConfiguration::Create(
            {.event_type = EventType::kUser, .destination = destination_})
            .SetSourceInfo(std::move(source_info)),
        consumer_->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(1);
    helper_->mock_provider()->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateAndGetQueueWithSourceVersion) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    SourceInfo source_info;
    source_info.set_source(SourceInfo::ASH);
    source_info.set_source_version("1.0.0");
    ReportQueueFactory::Create(
        ReportQueueConfiguration::Create(
            {.event_type = EventType::kUser, .destination = destination_})
            .SetSourceInfo(std::move(source_info)),
        consumer_->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(1);
    helper_->mock_provider()->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateQueueWithSourceVersionAndInvalidSource) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  SourceInfo source_info;
  source_info.set_source(SourceInfo::SOURCE_UNSPECIFIED);
  source_info.set_source_version("1.0.0");
  ReportQueueFactory::Create(
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = destination_})
          .SetSourceInfo(std::move(source_info)),
      consumer_->GetReportQueueSetter(nullptr));
  // Expect failure before it gets to the report queue provider
  EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(0);
  // We do not expect the report queue to be existing in the consumer.
  EXPECT_FALSE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueue) {
  // Mock internal implementation to use a MockReportQueue
  helper_->mock_provider()
      ->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(1);
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kDevice, .destination = destination_}));
  EXPECT_THAT(report_queue, NotNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithValidReservedSpace) {
  // Mock internal implementation to use a MockReportQueue
  helper_->mock_provider()
      ->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(1);
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                        .destination = destination_,
                                        .reserved_space = 12345L}));
  EXPECT_THAT(report_queue, NotNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithInvalidReservedSpace) {
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                        .destination = destination_,
                                        .reserved_space = -1L}));
  EXPECT_THAT(report_queue, IsNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithInvalidConfig) {
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kDevice,
           .destination = Destination::UNDEFINED_DESTINATION}));
  EXPECT_THAT(report_queue, IsNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithSource) {
  // Mock internal implementation to use a MockReportQueue
  helper_->mock_provider()
      ->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(1);
  SourceInfo source_info;
  source_info.set_source(SourceInfo::ASH);
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = destination_})
          .SetSourceInfo(std::move(source_info)));
  EXPECT_THAT(report_queue, NotNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithSourceVersion) {
  // Mock internal implementation to use a MockReportQueue
  helper_->mock_provider()
      ->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(1);
  SourceInfo source_info;
  source_info.set_source(SourceInfo::ASH);
  source_info.set_source_version("1.0.0");
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = destination_})
          .SetSourceInfo(std::move(source_info)));
  EXPECT_THAT(report_queue, NotNull());
}

TEST_F(ReportQueueFactoryTest,
       CreateSpeculativeQueueWithSourceVersionAndInvalidSource) {
  SourceInfo source_info;
  source_info.set_source(SourceInfo::SOURCE_UNSPECIFIED);
  source_info.set_source_version("1.0.0");
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = destination_})
          .SetSourceInfo(std::move(source_info)));
  EXPECT_THAT(report_queue, IsNull());
}

// Tests if two consumers use the same provider and create two queues.
TEST_F(ReportQueueFactoryTest, SameProviderForMultipleThreads) {
  auto consumer2 = std::make_unique<MockReportQueueConsumer>();
  EXPECT_FALSE(consumer_->GetReportQueue());
  EXPECT_FALSE(consumer2->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    set_waiter.Attach();
    ReportQueueFactory::Create(
        ReportQueueConfiguration::Create(
            {.event_type = EventType::kDevice, .destination = destination_}),
        consumer_->GetReportQueueSetter(&set_waiter));
    ReportQueueFactory::Create(
        ReportQueueConfiguration::Create(
            {.event_type = EventType::kUser, .destination = destination_}),
        consumer2->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*helper_->mock_provider(), OnInitCompletedMock()).Times(1);
    helper_->mock_provider()->ExpectCreateNewQueueAndReturnNewMockQueue(2);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
  // And for the 2nd consumer
  EXPECT_TRUE(consumer2->GetReportQueue());
}

}  // namespace reporting
