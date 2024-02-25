// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/filtered_report_queue.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/test.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace reporting {
namespace {

constexpr char kTestMessage[] = "TEST_MESSAGE";

template <typename T>
class MockFilter : public FilteredReportQueue<T>::Filter {
 public:
  MOCK_METHOD(Status, is_accepted, (const T&), (override));
};

class FilteredReportQueueTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(FilteredReportQueueTest, StringAcceptedTest) {
  auto filter = std::make_unique<MockFilter<std::string>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<std::string>>(
      std::move(filter), std::move(actual_report_queue));

  EXPECT_CALL(*mock_filter, is_accepted(_))
      .WillOnce(Return(Status::StatusOK()));
  EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
      .WillOnce(
          Invoke([](ReportQueue::RecordProducer record_producer,
                    Priority priority, ReportQueue::EnqueueCallback callback) {
            std::move(callback).Run(Status::StatusOK());
          }));
  test::TestEvent<Status> enqueued;
  queue->Enqueue(kTestMessage, Priority::IMMEDIATE, enqueued.cb());
  EXPECT_OK(enqueued.result());
  EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
              UnorderedElementsAre(base::Bucket(false, 1)));
}

TEST_F(FilteredReportQueueTest, StringRejectedTest) {
  auto filter = std::make_unique<MockFilter<std::string>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<std::string>>(
      std::move(filter), std::move(actual_report_queue));

  EXPECT_CALL(*mock_filter, is_accepted(_))
      .WillOnce(Return(Status(error::CANCELLED, "Rejected in test")));
  EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _)).Times(0);
  test::TestEvent<Status> enqueued;
  queue->Enqueue(kTestMessage, Priority::IMMEDIATE, enqueued.cb());
  EXPECT_THAT(
      enqueued.result(),
      AllOf(Property(&Status::error_code, Eq(error::CANCELLED)),
            Property(&Status::error_message, StrEq("Rejected in test"))));
  EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
              UnorderedElementsAre(base::Bucket(true, 1)));
}

TEST_F(FilteredReportQueueTest, MixedStringsTest) {
  auto filter = std::make_unique<MockFilter<std::string>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<std::string>>(
      std::move(filter), std::move(actual_report_queue));

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status::StatusOK()));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
        .WillOnce(Invoke([](ReportQueue::RecordProducer record_producer,
                            Priority priority,
                            ReportQueue::EnqueueCallback callback) {
          std::move(callback).Run(Status::StatusOK());
        }));
    test::TestEvent<Status> enqueued;
    queue->Enqueue(kTestMessage, Priority::IMMEDIATE, enqueued.cb());
    EXPECT_OK(enqueued.result());
    EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
                UnorderedElementsAre(base::Bucket(false, 1)));
  }

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status(error::ALREADY_EXISTS, "Duplicated in test")));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _)).Times(0);
    test::TestEvent<Status> enqueued;
    queue->Enqueue(kTestMessage, Priority::IMMEDIATE, enqueued.cb());
    EXPECT_THAT(
        enqueued.result(),
        AllOf(Property(&Status::error_code, Eq(error::ALREADY_EXISTS)),
              Property(&Status::error_message, StrEq("Duplicated in test"))));
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
        UnorderedElementsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  }

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status::StatusOK()));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
        .WillOnce(Invoke([](ReportQueue::RecordProducer record_producer,
                            Priority priority,
                            ReportQueue::EnqueueCallback callback) {
          std::move(callback).Run(Status::StatusOK());
        }));
    test::TestEvent<Status> enqueued;
    queue->Enqueue(kTestMessage, Priority::IMMEDIATE, enqueued.cb());
    EXPECT_OK(enqueued.result());
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
        UnorderedElementsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  }
}

TEST_F(FilteredReportQueueTest, JsonAcceptedTest) {
  auto filter = std::make_unique<MockFilter<base::Value::Dict>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<base::Value::Dict>>(
      std::move(filter), std::move(actual_report_queue));

  static constexpr char kTestKey[] = "TEST_KEY";
  static constexpr char kTestValue[] = "TEST_VALUE";
  base::Value::Dict test_dict;
  test_dict.Set(kTestKey, kTestValue);

  EXPECT_CALL(*mock_filter, is_accepted(_))
      .WillOnce(Return(Status::StatusOK()));
  EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
      .WillOnce(
          Invoke([](ReportQueue::RecordProducer record_producer,
                    Priority priority, ReportQueue::EnqueueCallback callback) {
            std::move(callback).Run(Status::StatusOK());
          }));
  test::TestEvent<Status> enqueued;
  queue->Enqueue(std::move(test_dict), Priority::IMMEDIATE, enqueued.cb());
  EXPECT_OK(enqueued.result());
  EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
              UnorderedElementsAre(base::Bucket(false, 1)));
}

TEST_F(FilteredReportQueueTest, JsonRejectedTest) {
  auto filter = std::make_unique<MockFilter<base::Value::Dict>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<base::Value::Dict>>(
      std::move(filter), std::move(actual_report_queue));

  static constexpr char kTestKey[] = "TEST_KEY";
  static constexpr char kTestValue[] = "TEST_VALUE";
  base::Value::Dict test_dict;
  test_dict.Set(kTestKey, kTestValue);

  EXPECT_CALL(*mock_filter, is_accepted(_))
      .WillOnce(Return(Status(error::CANCELLED, "Rejected in test")));
  EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _)).Times(0);
  test::TestEvent<Status> enqueued;
  queue->Enqueue(std::move(test_dict), Priority::IMMEDIATE, enqueued.cb());
  EXPECT_THAT(
      enqueued.result(),
      AllOf(Property(&Status::error_code, Eq(error::CANCELLED)),
            Property(&Status::error_message, StrEq("Rejected in test"))));
  EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
              UnorderedElementsAre(base::Bucket(true, 1)));
}

TEST_F(FilteredReportQueueTest, MixedJsonTest) {
  auto filter = std::make_unique<MockFilter<base::Value::Dict>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<base::Value::Dict>>(
      std::move(filter), std::move(actual_report_queue));

  static constexpr char kTestKey[] = "TEST_KEY";
  static constexpr char kTestValue[] = "TEST_VALUE";
  base::Value::Dict test_dict;
  test_dict.Set(kTestKey, kTestValue);

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status::StatusOK()));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
        .WillOnce(Invoke([](ReportQueue::RecordProducer record_producer,
                            Priority priority,
                            ReportQueue::EnqueueCallback callback) {
          std::move(callback).Run(Status::StatusOK());
        }));
    test::TestEvent<Status> enqueued;
    queue->Enqueue(test_dict.Clone(), Priority::IMMEDIATE, enqueued.cb());
    EXPECT_OK(enqueued.result());
    EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
                UnorderedElementsAre(base::Bucket(false, 1)));
  }

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status(error::ALREADY_EXISTS, "Duplicated in test")));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _)).Times(0);
    test::TestEvent<Status> enqueued;
    queue->Enqueue(test_dict.Clone(), Priority::IMMEDIATE, enqueued.cb());
    EXPECT_THAT(
        enqueued.result(),
        AllOf(Property(&Status::error_code, Eq(error::ALREADY_EXISTS)),
              Property(&Status::error_message, StrEq("Duplicated in test"))));
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
        UnorderedElementsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  }

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status::StatusOK()));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
        .WillOnce(Invoke([](ReportQueue::RecordProducer record_producer,
                            Priority priority,
                            ReportQueue::EnqueueCallback callback) {
          std::move(callback).Run(Status::StatusOK());
        }));
    test::TestEvent<Status> enqueued;
    queue->Enqueue(test_dict.Clone(), Priority::IMMEDIATE, enqueued.cb());
    EXPECT_OK(enqueued.result());
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
        UnorderedElementsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  }
}

TEST_F(FilteredReportQueueTest, ProtoAcceptedTest) {
  auto filter = std::make_unique<MockFilter<test::TestMessage>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<test::TestMessage>>(
      std::move(filter), std::move(actual_report_queue));

  test::TestMessage test_message;
  test_message.set_test(kTestMessage);

  EXPECT_CALL(*mock_filter, is_accepted(_))
      .WillOnce(Return(Status::StatusOK()));
  EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
      .WillOnce(
          Invoke([](ReportQueue::RecordProducer record_producer,
                    Priority priority, ReportQueue::EnqueueCallback callback) {
            std::move(callback).Run(Status::StatusOK());
          }));
  test::TestEvent<Status> enqueued;
  queue->Enqueue(std::move(test_message), Priority::IMMEDIATE, enqueued.cb());
  EXPECT_OK(enqueued.result());
  EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
              UnorderedElementsAre(base::Bucket(false, 1)));
}

TEST_F(FilteredReportQueueTest, ProtoRejectedTest) {
  auto filter = std::make_unique<MockFilter<test::TestMessage>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<test::TestMessage>>(
      std::move(filter), std::move(actual_report_queue));

  test::TestMessage test_message;
  test_message.set_test(kTestMessage);

  EXPECT_CALL(*mock_filter, is_accepted(_))
      .WillOnce(Return(Status(error::CANCELLED, "Rejected in test")));
  EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _)).Times(0);
  test::TestEvent<Status> enqueued;
  queue->Enqueue(std::move(test_message), Priority::IMMEDIATE, enqueued.cb());
  EXPECT_THAT(
      enqueued.result(),
      AllOf(Property(&Status::error_code, Eq(error::CANCELLED)),
            Property(&Status::error_message, StrEq("Rejected in test"))));
  EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
              UnorderedElementsAre(base::Bucket(true, 1)));
}

TEST_F(FilteredReportQueueTest, MixedProtoTest) {
  auto filter = std::make_unique<MockFilter<test::TestMessage>>();
  auto* const mock_filter = filter.get();

  auto actual_report_queue = std::make_unique<MockReportQueue>();
  auto* const mock_report_queue = actual_report_queue.get();
  auto queue = std::make_unique<FilteredReportQueue<test::TestMessage>>(
      std::move(filter), std::move(actual_report_queue));

  test::TestMessage test_message;
  test_message.set_test(kTestMessage);

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status::StatusOK()));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
        .WillOnce(Invoke([](ReportQueue::RecordProducer record_producer,
                            Priority priority,
                            ReportQueue::EnqueueCallback callback) {
          std::move(callback).Run(Status::StatusOK());
        }));
    test::TestEvent<Status> enqueued;
    queue->Enqueue(test_message, Priority::IMMEDIATE, enqueued.cb());
    EXPECT_OK(enqueued.result());
    EXPECT_THAT(histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
                UnorderedElementsAre(base::Bucket(false, 1)));
  }

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status(error::ALREADY_EXISTS, "Duplicated in test")));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _)).Times(0);
    test::TestEvent<Status> enqueued;
    queue->Enqueue(test_message, Priority::IMMEDIATE, enqueued.cb());
    EXPECT_THAT(
        enqueued.result(),
        AllOf(Property(&Status::error_code, Eq(error::ALREADY_EXISTS)),
              Property(&Status::error_message, StrEq("Duplicated in test"))));
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
        UnorderedElementsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  }

  {
    EXPECT_CALL(*mock_filter, is_accepted(_))
        .WillOnce(Return(Status::StatusOK()));
    EXPECT_CALL(*mock_report_queue, AddProducedRecord(_, _, _))
        .WillOnce(Invoke([](ReportQueue::RecordProducer record_producer,
                            Priority priority,
                            ReportQueue::EnqueueCallback callback) {
          std::move(callback).Run(Status::StatusOK());
        }));
    test::TestEvent<Status> enqueued;
    queue->Enqueue(test_message, Priority::IMMEDIATE, enqueued.cb());
    EXPECT_OK(enqueued.result());
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(queue->kFilteredOutEventsUma),
        UnorderedElementsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  }
}
}  // namespace
}  // namespace reporting
