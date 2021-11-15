// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_report_queue.h"

#include <memory>
#include <string>

#include "base/strings/string_piece_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/metrics/fake_reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

using ::testing::_;

class MetricReportQueueTest : public ::testing::Test {
 public:
  void SetUp() override {
    priority_ = Priority::SLOW_BATCH;
    settings_ = std::make_unique<FakeReportingSettings>();
  }

 protected:
  const std::string kRateSettingPath = "rate_path";

  std::unique_ptr<FakeReportingSettings> settings_;

  Priority priority_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(MetricReportQueueTest, ManualFlush) {
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(
              base::ThreadPool::CreateSequencedTaskRunner({})));
  auto* mock_queue_ptr = mock_queue.get();
  MetricData record;
  record.set_timestamp_ms(123456);

  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_);

  EXPECT_CALL(*mock_queue_ptr, AddRecord(_, _, _))
      .WillOnce([&record, this](base::StringPiece record_string,
                                Priority actual_priority,
                                ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status());
        MetricData actual_record;

        EXPECT_TRUE(actual_record.ParseFromArray(record_string.data(),
                                                 record_string.size()));
        EXPECT_EQ(actual_record.timestamp_ms(), record.timestamp_ms());
        EXPECT_EQ(actual_priority, priority_);
      });
  bool callback_called = false;
  metric_report_queue.Enqueue(
      record, base::BindLambdaForTesting(
                  [&callback_called](Status) { callback_called = true; }));
  EXPECT_TRUE(callback_called);

  EXPECT_CALL(*mock_queue_ptr, Flush(priority_, _)).Times(1);
  metric_report_queue.Flush();
}

TEST_F(MetricReportQueueTest, RateControlledFlush_TimeNotElapsed) {
  constexpr int rate_ms = 10000;
  settings_->SetInteger(kRateSettingPath, rate_ms);
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(
              base::ThreadPool::CreateSequencedTaskRunner({})));
  auto* mock_queue_ptr = mock_queue.get();
  MetricData record;
  record.set_timestamp_ms(123456);

  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_,
                                        settings_.get(), kRateSettingPath,
                                        /*default_rate=*/base::Milliseconds(1));

  EXPECT_CALL(*mock_queue_ptr, AddRecord(_, _, _))
      .WillOnce([&record, this](base::StringPiece record_string,
                                Priority actual_priority,
                                ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status());
        MetricData actual_record;

        EXPECT_TRUE(actual_record.ParseFromArray(record_string.data(),
                                                 record_string.size()));
        EXPECT_EQ(actual_record.timestamp_ms(), record.timestamp_ms());
        EXPECT_EQ(actual_priority, priority_);
      });
  bool callback_called = false;
  metric_report_queue.Enqueue(
      record, base::BindLambdaForTesting(
                  [&callback_called](Status) { callback_called = true; }));
  EXPECT_TRUE(callback_called);

  EXPECT_CALL(*mock_queue_ptr, Flush).Times(0);
  task_environment_.FastForwardBy(base::Milliseconds(rate_ms - 1));
}

TEST_F(MetricReportQueueTest, RateControlledFlush_TimeElapsed) {
  constexpr int rate_ms = 10000;
  settings_->SetInteger(kRateSettingPath, rate_ms);
  auto mock_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new testing::StrictMock<::reporting::MockReportQueue>(),
          base::OnTaskRunnerDeleter(
              base::ThreadPool::CreateSequencedTaskRunner({})));
  auto* mock_queue_ptr = mock_queue.get();
  MetricData record;
  record.set_timestamp_ms(123456);

  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_,
                                        settings_.get(), kRateSettingPath,
                                        /*default_rate=*/base::Milliseconds(1));

  EXPECT_CALL(*mock_queue_ptr, AddRecord(_, _, _))
      .WillOnce([&record, this](base::StringPiece record_string,
                                Priority actual_priority,
                                ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status());
        MetricData actual_record;

        EXPECT_TRUE(actual_record.ParseFromArray(record_string.data(),
                                                 record_string.size()));
        EXPECT_EQ(actual_record.timestamp_ms(), record.timestamp_ms());
        EXPECT_EQ(actual_priority, priority_);
      });
  bool callback_called = false;
  metric_report_queue.Enqueue(
      record, base::BindLambdaForTesting(
                  [&callback_called](Status) { callback_called = true; }));
  EXPECT_TRUE(callback_called);

  EXPECT_CALL(*mock_queue_ptr, Flush(priority_, _)).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(rate_ms));
}
}  // namespace reporting
