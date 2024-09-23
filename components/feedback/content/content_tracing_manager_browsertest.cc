// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/content/content_tracing_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kFakeKey = "fake key";
const std::string kFakeValue = "fake value";
const char kTraceFilename[] = "tracing.zip";
const int kFakeTraceId = 1;

}  // namespace

class TestFeedbackUploader final : public feedback::FeedbackUploader {
 public:
  TestFeedbackUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FeedbackUploader(is_off_the_record,
                         state_path,
                         std::move(url_loader_factory)) {}
  TestFeedbackUploader(const TestFeedbackUploader&) = delete;
  TestFeedbackUploader& operator=(const TestFeedbackUploader&) = delete;

  ~TestFeedbackUploader() override = default;

  base::WeakPtr<FeedbackUploader> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestFeedbackUploader> weak_ptr_factory_{this};
};

class ContentTracingManagerBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
  }
};

class FeedbackDataBrowserTest : public content::ContentBrowserTest {
 public:
  FeedbackDataBrowserTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    uploader_ = std::make_unique<TestFeedbackUploader>(
        /*is_of_the_record=*/false, scoped_temp_dir_.GetPath(),
        test_shared_loader_factory_);
  }

  FeedbackDataBrowserTest(const FeedbackDataBrowserTest&) = delete;
  FeedbackDataBrowserTest& operator=(const FeedbackDataBrowserTest&) = delete;

  ~FeedbackDataBrowserTest() override = default;

 protected:
  scoped_refptr<feedback::FeedbackData> CreateFeedbackData() {
    return base::MakeRefCounted<feedback::FeedbackData>(
        uploader_->AsWeakPtr(), ContentTracingManager::Get());
  }

  base::ScopedTempDir scoped_temp_dir_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<TestFeedbackUploader> uploader_;
};

// Test that the trace data is compressed successfully.
IN_PROC_BROWSER_TEST_F(ContentTracingManagerBrowserTest, TraceDataCompressed) {
  std::unique_ptr<ContentTracingManager> tracing_manager =
      ContentTracingManager::Create();
  EXPECT_NE(tracing_manager, nullptr);

  base::test::TestFuture<scoped_refptr<base::RefCountedString>> future;

  int trace_id = tracing_manager->RequestTrace();
  EXPECT_EQ(trace_id, 1);
  tracing_manager->GetTraceData(trace_id, future.GetCallback());
  // The actual trace data varies.
  EXPECT_TRUE(future.Get()->size() > 0u);
}

// Test that FeedbackData.CompressSystemInfo will not gather trace data when the
// performance tracing is not enabled.
IN_PROC_BROWSER_TEST_F(FeedbackDataBrowserTest, TraceNotEnabled) {
  auto* tracing_manager = ContentTracingManager::Get();
  EXPECT_EQ(tracing_manager, nullptr);

  scoped_refptr<feedback::FeedbackData> feedback_data = CreateFeedbackData();

  feedback_data->AddLog(kFakeKey, kFakeValue);
  // Set a trace id so that Feedback data will try to fetch trace data.
  feedback_data->set_trace_id(kFakeTraceId);

  base::RunLoop run_loop;
  feedback_data->CompressSystemInfo();
  run_loop.RunUntilIdle();

  const std::map<std::string, std::string>* sys_info =
      feedback_data->sys_info();
  EXPECT_NE(sys_info, nullptr);
  EXPECT_EQ(sys_info->size(), 1u);
  // Verify that there is no trace file added.
  EXPECT_EQ(sys_info->count(kTraceFilename), 0u);
}

// Test that FeedbackData.CompressSystemInfo will not gather trace data when:
//  1) the performance tracing is enabled when feedback data is constructed, but
//  2) the tracing manager was reset before feedback data collects trace data.
// The code before this CL will crash.
IN_PROC_BROWSER_TEST_F(FeedbackDataBrowserTest, TraceEnabledLaterWasReset) {
  std::unique_ptr<ContentTracingManager> tracing_manager =
      ContentTracingManager::Create();
  EXPECT_NE(ContentTracingManager::Get(), nullptr);

  int trace_id = tracing_manager->RequestTrace();
  EXPECT_EQ(trace_id, 1);

  scoped_refptr<feedback::FeedbackData> feedback_data = CreateFeedbackData();

  feedback_data->AddLog(kFakeKey, kFakeValue);
  // Set a trace id so that Feedback data will try to fetch trace data.
  feedback_data->set_trace_id(trace_id);

  tracing_manager.reset();
  EXPECT_EQ(tracing_manager, nullptr);
  EXPECT_EQ(ContentTracingManager::Get(), nullptr);

  base::RunLoop run_loop;
  feedback_data->CompressSystemInfo();
  run_loop.RunUntilIdle();

  const std::map<std::string, std::string>* sys_info =
      feedback_data->sys_info();
  EXPECT_NE(sys_info, nullptr);
  EXPECT_EQ(sys_info->size(), 1u);
  // Verify that there is not a trace file added.
  EXPECT_EQ(sys_info->count(kTraceFilename), 0u);
}
