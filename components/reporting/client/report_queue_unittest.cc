// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue.h"

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;

namespace reporting {
namespace {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//       base::OnceCallback<void(ResType* res)> type which also may perform some
//       other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//       collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~TestEvent() { EXPECT_FALSE(run_loop_->running()) << "Not responded"; }
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_->Run();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    return base::BindOnce(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(run_loop_.get()), base::Unretained(&result_));
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ResType result_;
};

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
  TestEvent<Status> e;
  queue.Enqueue("Record", FAST_BATCH, e.cb());
  ASSERT_OK(e.result());
}

}  // namespace
}  // namespace reporting
