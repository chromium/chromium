// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::WithArg;

namespace reporting {
namespace {

class ReportQueueTest : public ::testing::Test {
 protected:
  ReportQueueTest() = default;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;
};

TEST_F(ReportQueueTest, EnqueueTest) {
  MockReportQueue queue;
  EXPECT_CALL(queue, AddRecord(_, _, _))
      .WillOnce(WithArg<2>(Invoke([](ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status::StatusOK());
      })));
  test::TestEvent<Status> e;
  queue.Enqueue("Record", FAST_BATCH, e.cb());
  ASSERT_OK(e.result());
  histogram_tester_.ExpectBucketCount(ReportQueue::kEnqueueMetricsName,
                                      error::OK,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(ReportQueue::kEnqueueMetricsName,
                                     /*count=*/1);
}

TEST_F(ReportQueueTest, EnqueueWithErrorTest) {
  MockReportQueue queue;
  EXPECT_CALL(queue, AddRecord(_, _, _))
      .WillOnce(WithArg<2>(Invoke([](ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status(error::CANCELLED, "Cancelled by test"));
      })));
  test::TestEvent<Status> e;
  queue.Enqueue("Record", FAST_BATCH, e.cb());
  const auto result = e.result();
  ASSERT_FALSE(result.ok());
  ASSERT_THAT(result.error_code(), Eq(error::CANCELLED));
  histogram_tester_.ExpectBucketCount(ReportQueue::kEnqueueMetricsName,
                                      error::CANCELLED,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(ReportQueue::kEnqueueMetricsName,
                                     /*count=*/1);
}

TEST_F(ReportQueueTest, FlushTest) {
  MockReportQueue queue;
  EXPECT_CALL(queue, Flush(_, _))
      .WillOnce(WithArg<1>(Invoke([](ReportQueue::FlushCallback cb) {
        std::move(cb).Run(Status::StatusOK());
      })));
  test::TestEvent<Status> e;
  queue.Flush(MANUAL_BATCH, e.cb());
  ASSERT_OK(e.result());
}
}  // namespace
}  // namespace reporting
