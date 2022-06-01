// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_uploader.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/enterprise/common/proto/extensions_workflow_events.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StrEq;
using ::testing::WithArg;

namespace enterprise_reporting {
namespace {
constexpr char kDMToken[] = "dm-token";
constexpr char kQueueConfigurationCreationMetricsName[] =
    "Enterprise.CBCMRealTimeReportQueueConfigurationCreation";
constexpr char kQueueCreationMetricsName[] =
    "Enterprise.CBCMRealTimeReportQueueCreation";
constexpr char kRequestEnqueueMetricsName[] =
    "Enterprise.CBCMRealTimeReportEnqueue";

constexpr reporting::Priority kPriority = reporting::Priority::FAST_BATCH;

class FakeRealTimeUploader : public RealTimeUploader {
 public:
  explicit FakeRealTimeUploader(reporting::Priority priority)
      : RealTimeUploader(priority),
        mock_report_queue_(std::make_unique<reporting::MockReportQueue>()),
        mock_report_queue_ptr_(mock_report_queue_.get()) {}
  ~FakeRealTimeUploader() override = default;

  // RealTimeUploader
  void CreateReportQueueRequest(
      reporting::StatusOr<std::unique_ptr<reporting::ReportQueueConfiguration>>
          config,
      reporting::ReportQueueProvider::CreateReportQueueCallback callback)
      override {
    DCHECK(callback);

    reporting::ReportQueueProvider::CreateReportQueueResponse response;
    if (code_ == reporting::error::OK) {
      response = std::move(mock_report_queue_);
    } else {
      response = reporting::Status(code_, "");
    }
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
  }

  reporting::MockReportQueue* mock_report_queue() {
    DCHECK(mock_report_queue_ptr_);
    return mock_report_queue_ptr_;
  }

  void SetError(reporting::error::Code code) { code_ = code; }

 private:
  reporting::error::Code code_ = reporting::error::OK;
  std::unique_ptr<reporting::MockReportQueue> mock_report_queue_;
  raw_ptr<reporting::MockReportQueue> mock_report_queue_ptr_;
};
}  // namespace

class RealTimeUploaderTest : public ::testing::Test {
 public:
  RealTimeUploaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  std::unique_ptr<ExtensionsWorkflowEvent> CreateReportAndGetSerializedString(
      const std::string& id,
      std::string* serialized_string) {
    auto report = std::make_unique<ExtensionsWorkflowEvent>();
    report->set_id(id);
    report->SerializeToString(serialized_string);
    return report;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeRealTimeUploader> uploader_;
  base::MockCallback<RealTimeUploader::EnqueueCallback> mock_enqueue_callback_;
  base::HistogramTester histogram_tester_;
};

TEST_F(RealTimeUploaderTest, CreateReportQueue) {
  uploader_ = std::make_unique<FakeRealTimeUploader>(kPriority);
  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);

  EXPECT_FALSE(uploader_->IsEnabled());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(uploader_->IsEnabled());
  histogram_tester_.ExpectUniqueSample(kQueueConfigurationCreationMetricsName,
                                       reporting::error::OK,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kQueueCreationMetricsName,
                                       reporting::error::OK,
                                       /*expected_bucket_count=*/1);
}

TEST_F(RealTimeUploaderTest, CreateReportQueueAndFailed) {
  uploader_ = std::make_unique<FakeRealTimeUploader>(kPriority);
  uploader_->SetError(reporting::error::UNKNOWN);
  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);
  EXPECT_FALSE(uploader_->IsEnabled());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(uploader_->IsEnabled());
  histogram_tester_.ExpectUniqueSample(kQueueConfigurationCreationMetricsName,
                                       reporting::error::OK,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kQueueCreationMetricsName,
                                       reporting::error::UNKNOWN,
                                       /*expected_bucket_count=*/1);
}

TEST_F(RealTimeUploaderTest, CreateReportQueueAndCancel) {
  uploader_ = std::make_unique<FakeRealTimeUploader>(kPriority);
  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);
  // uploader is deleted before the report queue is created.
  uploader_.reset();

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(kQueueConfigurationCreationMetricsName,
                                       reporting::error::OK,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(kQueueCreationMetricsName, /*count=*/0);
}

TEST_F(RealTimeUploaderTest, UploadReport) {
  uploader_ = std::make_unique<FakeRealTimeUploader>(kPriority);
  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);
  task_environment_.RunUntilIdle();

  std::string expected_report_1;
  auto report_1 =
      CreateReportAndGetSerializedString("id-1", &expected_report_1);
  std::string expected_report_2;
  auto report_2 =
      CreateReportAndGetSerializedString("id-2", &expected_report_2);
  {
    InSequence sequence;

    EXPECT_CALL(*uploader_->mock_report_queue(),
                AddRecord(StrEq(expected_report_1), kPriority, _))
        .Times(1)
        .WillOnce(WithArg<2>(
            Invoke([](reporting::ReportQueue::EnqueueCallback callback) {
              base::ThreadPool::PostTask(
                  base::BindOnce(std::move(callback),
                                 reporting::Status(reporting::error::OK, "")));
            })));

    EXPECT_CALL(*uploader_->mock_report_queue(),
                AddRecord(StrEq(expected_report_2), kPriority, _))
        .Times(1)
        .WillOnce(WithArg<2>(
            Invoke([](reporting::ReportQueue::EnqueueCallback callback) {
              base::ThreadPool::PostTask(base::BindOnce(
                  std::move(callback),
                  reporting::Status(reporting::error::UNKNOWN, "")));
            })));
  }

  EXPECT_CALL(mock_enqueue_callback_, Run(true)).Times(1);
  EXPECT_CALL(mock_enqueue_callback_, Run(false)).Times(1);

  uploader_->Upload(std::move(report_1), mock_enqueue_callback_.Get());
  uploader_->Upload(std::move(report_2), mock_enqueue_callback_.Get());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(kRequestEnqueueMetricsName,
                                      reporting::error::OK,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kRequestEnqueueMetricsName,
                                      reporting::error::UNKNOWN,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(kRequestEnqueueMetricsName, /*count=*/2);
}

TEST_F(RealTimeUploaderTest, UploadReportBeforeQueueIsReady) {
  uploader_ = std::make_unique<FakeRealTimeUploader>(kPriority);

  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);

  // Does not call RunUntilIdle so that the queue is not created.
  std::string expected_report;
  auto report = CreateReportAndGetSerializedString("id", &expected_report);

  EXPECT_CALL(*uploader_->mock_report_queue(),

              AddRecord(StrEq(expected_report), kPriority, _))
      .Times(1)
      .WillOnce(WithArg<2>(
          Invoke([](reporting::ReportQueue::EnqueueCallback callback) {
            base::ThreadPool::PostTask(
                base::BindOnce(std::move(callback),
                               reporting::Status(reporting::error::OK, "")));
          })));

  EXPECT_CALL(mock_enqueue_callback_, Run(true)).Times(1);
  EXPECT_CALL(mock_enqueue_callback_, Run(false)).Times(0);
  uploader_->Upload(std::move(report), mock_enqueue_callback_.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(kRequestEnqueueMetricsName,
                                       reporting::error::OK,
                                       /*expected_bucket_count=*/1);
}

}  // namespace enterprise_reporting
