// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_data.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

namespace {

constexpr char kHistograms[] = "Histogram Data";
constexpr char kImageData[] = "Image Data";
constexpr char kFileData[] = "File Data";

class MockUploader : public FeedbackUploader {
 public:
  MockUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      content::BrowserContext* context,
      base::OnceClosure on_report_sent)
      : FeedbackUploader(context,
                         FeedbackUploaderFactory::CreateUploaderTaskRunner()),
        on_report_sent_(std::move(on_report_sent)) {
    set_url_loader_factory_for_test(url_loader_factory);
  }
  ~MockUploader() override {}

  // feedback::FeedbackUploader:
  void StartDispatchingReport() override { std::move(on_report_sent_).Run(); }

 private:
  base::OnceClosure on_report_sent_;

  DISALLOW_COPY_AND_ASSIGN(MockUploader);
};

}  // namespace

class FeedbackDataTest : public testing::Test {
 protected:
  FeedbackDataTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        uploader_(test_shared_loader_factory_,
                  &context_,
                  base::BindOnce(&FeedbackDataTest::set_send_report_callback,
                                 base::Unretained(this))),
        data_(base::MakeRefCounted<FeedbackData>(&uploader_)) {}
  ~FeedbackDataTest() override = default;

  void Send() {
    bool attached_file_completed =
        data_->attached_file_uuid().empty();
    bool screenshot_completed =
        data_->screenshot_uuid().empty();

    if (screenshot_completed && attached_file_completed) {
      data_->OnFeedbackPageDataComplete();
    }
  }

  void RunMessageLoop() {
    run_loop_.reset(new base::RunLoop());
    quit_closure_ = run_loop_->QuitClosure();
    run_loop_->Run();
  }

  void set_send_report_callback() { quit_closure_.Run(); }

  base::Closure quit_closure_;
  std::unique_ptr<base::RunLoop> run_loop_;
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  content::TestBrowserContext context_;
  MockUploader uploader_;
  scoped_refptr<FeedbackData> data_;
};

TEST_F(FeedbackDataTest, ReportSending) {
  data_->SetAndCompressHistograms(kHistograms);
  data_->set_image(kImageData);
  data_->AttachAndCompressFileData(kFileData);
  Send();
  RunMessageLoop();
  EXPECT_TRUE(data_->IsDataComplete());
}

}  // namespace feedback
