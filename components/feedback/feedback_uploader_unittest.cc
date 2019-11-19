// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

namespace {

constexpr char kReportOne[] = "one";
constexpr char kReportTwo[] = "two";
constexpr char kReportThree[] = "three";
constexpr char kReportFour[] = "four";
constexpr char kReportFive[] = "five";

constexpr base::TimeDelta kRetryDelayForTest =
    base::TimeDelta::FromMilliseconds(100);

class MockFeedbackUploader : public FeedbackUploader {
 public:
  MockFeedbackUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      content::BrowserContext* context)
      : FeedbackUploader(context,
                         FeedbackUploaderFactory::CreateUploaderTaskRunner()) {
    set_url_loader_factory_for_test(url_loader_factory);
  }
  ~MockFeedbackUploader() override {}

  void RunMessageLoop() {
    if (ProcessingComplete())
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void SimulateLoadingOfflineReports() {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FeedbackReport::LoadReportsAndQueue, feedback_reports_path(),
            base::BindRepeating(&MockFeedbackUploader::QueueSingleReport,
                                base::SequencedTaskRunnerHandle::Get(),
                                AsWeakPtr())));
  }

  const std::map<std::string, unsigned int>& dispatched_reports() const {
    return dispatched_reports_;
  }
  void set_expected_reports(size_t value) { expected_reports_ = value; }
  void set_simulate_failure(bool value) { simulate_failure_ = value; }

 private:
  static void QueueSingleReport(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      base::WeakPtr<FeedbackUploader> uploader,
      scoped_refptr<FeedbackReport> report) {
    main_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&MockFeedbackUploader::RequeueReport,
                                  std::move(uploader), std::move(report)));
  }

  // FeedbackUploaderChrome:
  void StartDispatchingReport() override {
    if (base::Contains(dispatched_reports_, report_being_dispatched()->data()))
      dispatched_reports_[report_being_dispatched()->data()]++;
    else
      dispatched_reports_[report_being_dispatched()->data()] = 1;

    dispatched_reports_count_++;

    if (simulate_failure_)
      OnReportUploadFailure(true /* should_retry */);
    else
      OnReportUploadSuccess();

    if (ProcessingComplete()) {
      if (run_loop_)
        run_loop_->Quit();
    }
  }

  bool ProcessingComplete() {
    return (dispatched_reports_count_ >= expected_reports_);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::map<std::string, unsigned int> dispatched_reports_;
  size_t dispatched_reports_count_ = 0;
  size_t expected_reports_ = 0;
  bool simulate_failure_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockFeedbackUploader);
};

}  // namespace

class FeedbackUploaderTest : public testing::Test {
 public:
  FeedbackUploaderTest() {
    FeedbackUploader::SetMinimumRetryDelayForTesting(kRetryDelayForTest);
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    RecreateUploader();
  }

  ~FeedbackUploaderTest() override = default;

  void RecreateUploader() {
    uploader_ = std::make_unique<MockFeedbackUploader>(
        test_shared_loader_factory_, &context_);
  }

  void QueueReport(const std::string& data) {
    uploader_->QueueReport(std::make_unique<std::string>(data));
  }

  MockFeedbackUploader* uploader() const { return uploader_.get(); }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  std::unique_ptr<MockFeedbackUploader> uploader_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderTest);
};

TEST_F(FeedbackUploaderTest, QueueMultiple) {
  QueueReport(kReportOne);
  QueueReport(kReportTwo);
  QueueReport(kReportThree);
  QueueReport(kReportFour);

  EXPECT_EQ(uploader()->dispatched_reports().size(), 4u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportOne), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportTwo), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportThree), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportFour), 1u);
}

TEST_F(FeedbackUploaderTest, QueueMultipleWithFailures) {
  EXPECT_EQ(kRetryDelayForTest, uploader()->retry_delay());
  QueueReport(kReportOne);

  // Simulate a failure in reports two and three. Make sure the backoff delay
  // will be applied twice, and the reports will eventually be sent.
  uploader()->set_simulate_failure(true);
  QueueReport(kReportTwo);
  EXPECT_EQ(kRetryDelayForTest * 2, uploader()->retry_delay());
  QueueReport(kReportThree);
  EXPECT_EQ(kRetryDelayForTest * 4, uploader()->retry_delay());
  uploader()->set_simulate_failure(false);

  // Once a successful report is sent, the backoff delay is reset back to its
  // minimum value.
  QueueReport(kReportFour);
  EXPECT_EQ(kRetryDelayForTest, uploader()->retry_delay());
  QueueReport(kReportFive);

  // Wait for the pending two failed reports to be sent.
  uploader()->set_expected_reports(7);
  uploader()->RunMessageLoop();

  EXPECT_EQ(uploader()->dispatched_reports().size(), 5u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportOne), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportTwo), 2u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportThree), 2u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportFour), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportFive), 1u);
}

TEST_F(FeedbackUploaderTest, SimulateOfflineReports) {
  // Simulate offline reports by failing to upload three reports.
  uploader()->set_simulate_failure(true);
  QueueReport(kReportOne);
  QueueReport(kReportTwo);
  QueueReport(kReportThree);

  // All three reports will be attempted to be uploaded, but the uploader queue
  // will remain having three reports since they all failed.
  uploader()->set_expected_reports(3);
  uploader()->RunMessageLoop();
  EXPECT_EQ(uploader()->dispatched_reports().size(), 3u);
  EXPECT_FALSE(uploader()->QueueEmpty());

  // Simulate a sign out / resign in by recreating the uploader. This should not
  // clear any pending feedback report files on disk, and hence they can be
  // reloaded.
  RecreateUploader();
  uploader()->SimulateLoadingOfflineReports();
  uploader()->set_expected_reports(3);
  uploader()->RunMessageLoop();

  // The three reports were loaded, successfully uploaded, and the uploader
  // queue is now empty.
  EXPECT_EQ(uploader()->dispatched_reports().size(), 3u);
  EXPECT_TRUE(uploader()->QueueEmpty());
}

}  // namespace feedback
