// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/log_uploader.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

namespace {

const char kTestServerURL[] = "http://a.com/";
const char kTestMimeType[] = "text/plain";

class TestLogUploader : public LogUploader {
 public:
  explicit TestLogUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : LogUploader(GURL(kTestServerURL), kTestMimeType, url_loader_factory) {
    Start();
  }

  base::TimeDelta last_interval_set() const { return last_interval_set_; }

  void StartUpload() {
    last_interval_set_ = base::TimeDelta();
    StartScheduledUpload();
  }

  static base::TimeDelta BackOff(base::TimeDelta t) {
    return LogUploader::BackOffUploadInterval(t);
  }

 protected:
  bool IsUploadScheduled() const override {
    return !last_interval_set().is_zero();
  }

  // Schedules a future call to StartScheduledUpload if one isn't already
  // pending.
  void ScheduleNextUpload(base::TimeDelta interval) override {
    EXPECT_EQ(last_interval_set(), base::TimeDelta());
    last_interval_set_ = interval;
  }

  base::TimeDelta last_interval_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLogUploader);
};

}  // namespace

class LogUploaderTest : public testing::Test {
 public:
  LogUploaderTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

 protected:
  // Required for base::ThreadTaskRunnerHandle::Get().
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogUploaderTest);
};

TEST_F(LogUploaderTest, Success) {
  TestLogUploader uploader(test_url_loader_factory_.GetSafeWeakWrapper());
  test_url_loader_factory_.AddResponse(kTestServerURL, "");

  uploader.QueueLog("log1");
  base::RunLoop().RunUntilIdle();
  // Log should be discarded instead of retransmitted.
  EXPECT_EQ(uploader.last_interval_set(), base::TimeDelta());
}

TEST_F(LogUploaderTest, Rejection) {
  TestLogUploader uploader(test_url_loader_factory_.GetSafeWeakWrapper());

  auto response_head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 400 Bad Request\nContent-type: text/html\n\n");
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  response_head->mime_type = "text/html";
  test_url_loader_factory_.AddResponse(GURL(kTestServerURL),
                                       std::move(response_head), "",
                                       network::URLLoaderCompletionStatus());

  uploader.QueueLog("log1");
  base::RunLoop().RunUntilIdle();
  // Log should be discarded instead of retransmitted.
  EXPECT_EQ(uploader.last_interval_set(), base::TimeDelta());
}

TEST_F(LogUploaderTest, Failure) {
  TestLogUploader uploader(test_url_loader_factory_.GetSafeWeakWrapper());

  auto response_head = network::mojom::URLResponseHead::New();
  std::string headers(
      "HTTP/1.1 500 Internal Server Error\nContent-type: text/html\n\n");
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  response_head->mime_type = "text/html";
  test_url_loader_factory_.AddResponse(GURL(kTestServerURL),
                                       std::move(response_head), "",
                                       network::URLLoaderCompletionStatus());

  uploader.QueueLog("log1");
  base::RunLoop().RunUntilIdle();
  // Log should be scheduled for retransmission.
  base::TimeDelta error_interval = uploader.last_interval_set();
  EXPECT_GT(error_interval, base::TimeDelta());

  for (int i = 0; i < 10; i++) {
    uploader.QueueLog("logX");
  }

  // A second failure should lead to a longer interval, and the log should
  // be discarded due to full queue.
  uploader.StartUpload();
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(uploader.last_interval_set(), error_interval);

  test_url_loader_factory_.AddResponse(kTestServerURL, "");

  // A success should revert to base interval while queue is not empty.
  for (int i = 0; i < 9; i++) {
    uploader.StartUpload();
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(uploader.last_interval_set(), error_interval);
  }

  // Queue should be empty.
  uploader.StartUpload();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(uploader.last_interval_set(), base::TimeDelta());
}

TEST_F(LogUploaderTest, Backoff) {
  base::TimeDelta current = base::TimeDelta();
  base::TimeDelta next = base::TimeDelta::FromSeconds(1);
  // Backoff until the maximum is reached.
  while (next > current) {
    current = next;
    next = TestLogUploader::BackOff(current);
  }
  // Maximum backoff should have been reached.
  EXPECT_EQ(next, current);
}

}  // namespace rappor
