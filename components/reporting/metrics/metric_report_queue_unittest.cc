// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_report_queue.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

namespace reporting {
namespace {

constexpr char kRateSettingPath[] = "rate_path";
constexpr int kRateMs = 10000;
constexpr base::TimeDelta kDefaultRate = base::Milliseconds(100);

class MetricReportQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    priority_ = Priority::SLOW_BATCH;
    settings_ = std::make_unique<test::FakeReportingSettings>();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<test::FakeReportingSettings> settings_;
  Priority priority_;
};

TEST_F(MetricReportQueueTest, ManualUpload) {
  auto mock_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new MockReportQueue(),
      base::OnTaskRunnerDeleter(
          base::ThreadPool::CreateSequencedTaskRunner({})));
  auto* mock_queue_ptr = mock_queue.get();
  MetricData record;
  record.set_timestamp_ms(123456);

  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_);
  EXPECT_CALL(*mock_queue_ptr, AddRecord(_, _, _))
      .WillOnce([&record, this](std::string_view record_string,
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
  metric_report_queue.Upload();
}

TEST_F(MetricReportQueueTest, ManualUploadWithTimer) {
  settings_->SetInteger(kRateSettingPath, kRateMs);

  int upload_count = 0;
  auto mock_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new MockReportQueue(),
      base::OnTaskRunnerDeleter(
          base::ThreadPool::CreateSequencedTaskRunner({})));
  auto* mock_queue_ptr = mock_queue.get();
  MetricData record;
  record.set_timestamp_ms(123456);

  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_,
                                        settings_.get(), kRateSettingPath,
                                        kDefaultRate);

  EXPECT_CALL(*mock_queue_ptr, AddRecord(_, _, _))
      .WillOnce([&record, this](std::string record_string,
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

  ON_CALL(*mock_queue_ptr, Flush(priority_, _)).WillByDefault([&]() {
    ++upload_count;
  });
  task_environment_.FastForwardBy(base::Milliseconds(kRateMs / 2));
  metric_report_queue.Upload();
  ASSERT_EQ(upload_count, 1);

  // Manual upload should have reset the timer so no upload should be expected
  // after the time is elapsed.
  task_environment_.FastForwardBy(base::Milliseconds(kRateMs / 2));
  ASSERT_EQ(upload_count, 1);

  // Full time elapsed after manual upload, new upload should be initiated.
  task_environment_.FastForwardBy(base::Milliseconds(kRateMs / 2));
  ASSERT_EQ(upload_count, 2);
}

TEST_F(MetricReportQueueTest, RateControlledFlush_TimeNotElapsed) {
  settings_->SetInteger(kRateSettingPath, kRateMs);
  auto mock_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new MockReportQueue(),
      base::OnTaskRunnerDeleter(
          base::ThreadPool::CreateSequencedTaskRunner({})));
  auto* mock_queue_ptr = mock_queue.get();
  MetricData record;
  record.set_timestamp_ms(123456);

  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_,
                                        settings_.get(), kRateSettingPath,
                                        kDefaultRate);

  EXPECT_CALL(*mock_queue_ptr, AddRecord(_, _, _))
      .WillOnce([&record, this](std::string record_string,
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
  task_environment_.FastForwardBy(base::Milliseconds(kRateMs - 1));
}

TEST_F(MetricReportQueueTest, RateControlledFlush_TimeElapsed) {
  settings_->SetInteger(kRateSettingPath, kRateMs);
  auto mock_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new MockReportQueue(),
      base::OnTaskRunnerDeleter(
          base::ThreadPool::CreateSequencedTaskRunner({})));
  auto* mock_queue_ptr = mock_queue.get();
  MetricData record;
  record.set_timestamp_ms(123456);

  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_,
                                        settings_.get(), kRateSettingPath,
                                        kDefaultRate);

  EXPECT_CALL(*mock_queue_ptr, AddRecord(_, _, _))
      .WillOnce([&record, this](std::string record_string,
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
  task_environment_.FastForwardBy(base::Milliseconds(kRateMs));
}

TEST_F(MetricReportQueueTest, GetDestination) {
  auto mock_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new MockReportQueue(),
      base::OnTaskRunnerDeleter(
          base::ThreadPool::CreateSequencedTaskRunner({})));
  const auto* const mock_queue_ptr = mock_queue.get();
  MetricReportQueue metric_report_queue(std::move(mock_queue), priority_);

  // Stub mock report queue to verify retrieved destination.
  const Destination destination = Destination::TELEMETRY_METRIC;
  EXPECT_CALL(*mock_queue_ptr, GetDestination()).WillOnce(Return(destination));
  EXPECT_THAT(metric_report_queue.GetDestination(), Eq(destination));
}
}  // namespace
}  // namespace reporting
