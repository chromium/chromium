// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_uploader.h"

#include <memory>

#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/enterprise/common/proto/extensions_workflow_events.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue_provider_test_helper.h"
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
constexpr char kRequestEnqueueMetricsName[] =
    "Enterprise.CBCMRealTimeReportEnqueue";

constexpr reporting::Priority kPriority = reporting::Priority::FAST_BATCH;

}  // namespace

class RealTimeUploaderTest : public ::testing::Test {
 public:
  RealTimeUploaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    helper_ =
        std::make_unique<reporting::test::ReportQueueProviderTestHelper>();
  }

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
  std::unique_ptr<RealTimeUploader> uploader_;
  std::unique_ptr<reporting::test::ReportQueueProviderTestHelper> helper_;
  base::MockCallback<RealTimeUploader::EnqueueCallback> mock_enqueue_callback_;
  base::HistogramTester histogram_tester_;
};

TEST_F(RealTimeUploaderTest, UploadReport) {
  helper_->mock_provider()
      ->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(1);

  uploader_ = RealTimeUploader::Create(
      kDMToken, reporting::Destination::EXTENSIONS_WORKFLOW, kPriority);

  std::string expected_report_1;
  auto report_1 =
      CreateReportAndGetSerializedString("id-1", &expected_report_1);
  std::string expected_report_2;
  auto report_2 =
      CreateReportAndGetSerializedString("id-2", &expected_report_2);
  {
    InSequence sequence;

    reporting::MockReportQueue* mock_report_queue =
        static_cast<reporting::MockReportQueue*>(uploader_->GetReportQueue());

    EXPECT_CALL(*mock_report_queue,
                AddRecord(StrEq(expected_report_1), kPriority, _))
        .Times(1)
        .WillOnce(WithArg<2>(
            Invoke([](reporting::ReportQueue::EnqueueCallback callback) {
              base::ThreadPool::PostTask(
                  base::BindOnce(std::move(callback),
                                 reporting::Status(reporting::error::OK, "")));
            })));

    EXPECT_CALL(*mock_report_queue,
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

}  // namespace enterprise_reporting
