// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue.h"

#include "base/bind.h"
#include "base/strings/strcat.h"
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
using ::testing::Invoke;
using ::testing::WithArg;

namespace reporting {
namespace {

class ReportQueueTest : public ::testing::Test {
 protected:
  ReportQueueTest() = default;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
