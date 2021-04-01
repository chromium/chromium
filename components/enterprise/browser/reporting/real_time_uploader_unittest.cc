// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_uploader.h"

#include <memory>

#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace {
constexpr char kDMToken[] = "dm-token";

class FakeRealTimeUploader : public RealTimeUploader {
 public:
  FakeRealTimeUploader() = default;
  ~FakeRealTimeUploader() override = default;

  // RealTimeUploader
  void CreateReportQueueRequest(
      reporting::StatusOr<std::unique_ptr<reporting::ReportQueueConfiguration>>
          config,
      reporting::ReportQueueProvider::CreateReportQueueCallback callback)
      override {
    if (!callback)
      return;

    reporting::ReportQueueProvider::CreateReportQueueResponse response;
    if (code_ == reporting::error::OK)
      response = std::make_unique<reporting::MockReportQueue>();
    else
      response = reporting::Status(code_, "");
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
  }

  void SetError(reporting::error::Code code) { code_ = code; }

 private:
  reporting::error::Code code_ = reporting::error::OK;
};
}  // namespace

class RealTimeUploaderTest : public ::testing::Test {
 public:
  RealTimeUploaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void CreateUploader() {
    uploader_ = std::make_unique<FakeRealTimeUploader>();
  }

 protected:
  std::unique_ptr<FakeRealTimeUploader> uploader_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(RealTimeUploaderTest, CreateReportQueue) {
  CreateUploader();
  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);

  EXPECT_FALSE(uploader_->IsEnabled());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(uploader_->IsEnabled());
}

TEST_F(RealTimeUploaderTest, CreateReportQueueAndFailed) {
  CreateUploader();
  uploader_->SetError(reporting::error::UNKNOWN);
  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);
  EXPECT_FALSE(uploader_->IsEnabled());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(uploader_->IsEnabled());
}

TEST_F(RealTimeUploaderTest, CreateReportQueueAndCancel) {
  CreateUploader();
  uploader_->CreateReportQueue(kDMToken,
                               reporting::Destination::EXTENSIONS_WORKFLOW);

  // uploader is deleted before the report queue is created.
  uploader_.reset();

  task_environment_.RunUntilIdle();
}

}  // namespace enterprise_reporting
