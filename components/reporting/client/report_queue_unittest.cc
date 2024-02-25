// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

namespace reporting {
namespace {

class ReportQueueTest : public ::testing::Test {
 protected:
  ReportQueueTest() = default;

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ReportQueueTest, EnqueueTest) {
  MockReportQueue queue;
  EXPECT_CALL(queue, AddRecord(_, _, _))
      .WillOnce(WithArg<2>(Invoke([](ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status::StatusOK());
      })));
  EXPECT_CALL(queue, GetDestination)
      .WillOnce(Return(Destination::EVENT_METRIC));
  base::test::TestFuture<Status> test_future;
  queue.Enqueue("Record", FAST_BATCH, test_future.GetCallback());
  ASSERT_OK(test_future.Take());
  histogram_tester_.ExpectBucketCount(ReportQueue::kEnqueueMetricsName,
                                      error::OK,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(ReportQueue::kEnqueueMetricsName,
                                     /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      ReportQueue::kEnqueueSuccessDestinationMetricsName,
      Destination::EVENT_METRIC,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      ReportQueue::kEnqueueSuccessDestinationMetricsName,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      ReportQueue::kEnqueueFailedDestinationMetricsName,
      /*expected_count=*/0);
}

TEST_F(ReportQueueTest, EnqueueWithErrorTest) {
  MockReportQueue queue;
  EXPECT_CALL(queue, AddRecord(_, _, _))
      .WillOnce(WithArg<2>(Invoke([](ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status(error::CANCELLED, "Cancelled by test"));
      })));
  EXPECT_CALL(queue, GetDestination)
      .WillOnce(Return(Destination::EVENT_METRIC));
  base::test::TestFuture<Status> test_future;
  queue.Enqueue("Record", FAST_BATCH, test_future.GetCallback());
  const auto result = test_future.Take();
  ASSERT_FALSE(result.ok());
  ASSERT_THAT(result.error_code(), Eq(error::CANCELLED));
  histogram_tester_.ExpectBucketCount(ReportQueue::kEnqueueMetricsName,
                                      error::CANCELLED,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(ReportQueue::kEnqueueMetricsName,
                                     /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      ReportQueue::kEnqueueFailedDestinationMetricsName,
      Destination::EVENT_METRIC,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      ReportQueue::kEnqueueFailedDestinationMetricsName,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      ReportQueue::kEnqueueSuccessDestinationMetricsName,
      /*expected_count=*/0);
}

TEST_F(ReportQueueTest, FlushTest) {
  MockReportQueue queue;
  EXPECT_CALL(queue, Flush(_, _))
      .WillOnce(WithArg<1>(Invoke([](ReportQueue::FlushCallback cb) {
        std::move(cb).Run(Status::StatusOK());
      })));
  base::test::TestFuture<Status> test_future;
  queue.Flush(MANUAL_BATCH, test_future.GetCallback());
  ASSERT_OK(test_future.Take());
}
}  // namespace
}  // namespace reporting
