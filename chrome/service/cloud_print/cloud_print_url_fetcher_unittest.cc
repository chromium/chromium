// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/service/service_process.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;

namespace cloud_print {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

int g_request_context_getter_instances = 0;
class TrackingTestURLRequestContextGetter
    : public net::TestURLRequestContextGetter {
 public:
  explicit TrackingTestURLRequestContextGetter(
      base::SingleThreadTaskRunner* io_task_runner,
      net::URLRequestThrottlerManager* throttler_manager)
      : TestURLRequestContextGetter(io_task_runner),
        throttler_manager_(throttler_manager) {
    g_request_context_getter_instances++;
  }

  net::TestURLRequestContext* GetURLRequestContext() override {
    if (!context_.get()) {
      context_.reset(new net::TestURLRequestContext(true));
      context_->set_throttler_manager(throttler_manager_);
      context_->Init();
    }
    return context_.get();
  }

 protected:
  ~TrackingTestURLRequestContextGetter() override {
    g_request_context_getter_instances--;
  }

 private:
  // Not owned here.
  net::URLRequestThrottlerManager* throttler_manager_;
  std::unique_ptr<net::TestURLRequestContext> context_;
};

class TestCloudPrintURLFetcher : public CloudPrintURLFetcher {
 public:
  explicit TestCloudPrintURLFetcher(
      base::SingleThreadTaskRunner* io_task_runner)
      : CloudPrintURLFetcher(PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS),
        io_task_runner_(io_task_runner) {}

  net::URLRequestContextGetter* GetRequestContextGetter() override {
    return new TrackingTestURLRequestContextGetter(io_task_runner_.get(),
                                                   throttler_manager());
  }

  net::URLRequestThrottlerManager* throttler_manager() {
    return &throttler_manager_;
  }

 private:
  ~TestCloudPrintURLFetcher() override {}

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // We set this as the throttler manager for the
  // TestURLRequestContext we create.
  net::URLRequestThrottlerManager throttler_manager_;
};

class CloudPrintURLFetcherTest : public testing::Test,
                                 public CloudPrintURLFetcher::Delegate {
 public:
  CloudPrintURLFetcherTest()
      : max_retries_(0),
        fetcher_(nullptr),
        quit_run_loop_(run_loop_.QuitClosure()) {}

  // Creates a URLFetcher, using the program's main thread to do IO.
  virtual void CreateFetcher(const GURL& url, int max_retries);

  // CloudPrintURLFetcher::Delegate
  CloudPrintURLFetcher::ResponseAction HandleRawResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const net::URLRequestStatus& status,
      int response_code,
      const std::string& data) override;

  CloudPrintURLFetcher::ResponseAction OnRequestAuthError() override {
    ADD_FAILURE();
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }

  std::string GetAuthHeader() override { return std::string(); }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() {
    return io_task_runner_;
  }

 protected:
  void SetUp() override {
    testing::Test::SetUp();

    io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  void TearDown() override {
    fetcher_.reset();
    // Deleting the fetcher causes a task to be posted to the IO thread to
    // release references to the URLRequestContextGetter. We need to run all
    // pending tasks to execute that (this is the IO thread).
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(0, g_request_context_getter_instances);
  }

  // URLFetcher is designed to run on the main UI thread, but in our tests
  // we assume that the current thread is the IO thread where the URLFetcher
  // dispatches its requests to.  When we wish to simulate being used from
  // a UI thread, we dispatch a worker thread to do so.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  int max_retries_;
  Time start_time_;
  scoped_refptr<TestCloudPrintURLFetcher> fetcher_;
  base::RunLoop run_loop_;
  base::OnceClosure quit_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintURLFetcherTest);
};

class CloudPrintURLFetcherBasicTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherBasicTest()
      : handle_raw_response_(false), handle_raw_data_(false) {}

  // CloudPrintURLFetcher::Delegate
  CloudPrintURLFetcher::ResponseAction HandleRawResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const net::URLRequestStatus& status,
      int response_code,
      const std::string& data) override;

  CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data) override;

  CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded) override;

  void SetHandleRawResponse(bool handle_raw_response) {
    handle_raw_response_ = handle_raw_response;
  }
  void SetHandleRawData(bool handle_raw_data) {
    handle_raw_data_ = handle_raw_data;
  }

 private:
  bool handle_raw_response_;
  bool handle_raw_data_;
};

// Version of CloudPrintURLFetcherTest that tests overload protection.
class CloudPrintURLFetcherOverloadTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherOverloadTest() : response_count_(0) {
  }

  // CloudPrintURLFetcher::Delegate
  CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data) override;

 private:
  int response_count_;
};

// Version of CloudPrintURLFetcherTest that tests backoff protection.
class CloudPrintURLFetcherRetryBackoffTest : public CloudPrintURLFetcherTest {
 public:
  CloudPrintURLFetcherRetryBackoffTest() : response_count_(0) {
  }

  // CloudPrintURLFetcher::Delegate
  CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data) override;

  void OnRequestGiveUp() override;

 private:
  int response_count_;
};


void CloudPrintURLFetcherTest::CreateFetcher(const GURL& url, int max_retries) {
  fetcher_ = new TestCloudPrintURLFetcher(io_task_runner().get());

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  scoped_refptr<net::URLRequestThrottlerEntry>
  entry(new net::URLRequestThrottlerEntry(
      fetcher_->throttler_manager(), std::string(), 200, 3, 1, 2.0, 0.0, 256));
  fetcher_->throttler_manager()->OverrideEntryForTests(url, entry.get());

  max_retries_ = max_retries;
  start_time_ = Time::Now();
  fetcher_->StartGetRequest(CloudPrintURLFetcher::REQUEST_MAX, url, this,
                            max_retries_, std::string());
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherTest::HandleRawResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleRawResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());

  if (handle_raw_response_) {
    // If the current message loop is not the IO loop, it will be shut down when
    // the main loop returns and this thread subsequently goes out of scope.
    std::move(quit_run_loop_).Run();
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  // We should never get here if we returned true in HandleRawResponse
  EXPECT_FALSE(handle_raw_response_);
  if (handle_raw_data_) {
    std::move(quit_run_loop_).Run();
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherBasicTest::HandleJSONData(const net::URLFetcher* source,
                                              const GURL& url,
                                              const base::Value& json_data,
                                              bool succeeded) {
  // We should never get here if we returned true in one of the above methods.
  EXPECT_FALSE(handle_raw_response_);
  EXPECT_FALSE(handle_raw_data_);
  std::move(quit_run_loop_).Run();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherOverloadTest::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  const TimeDelta one_second = TimeDelta::FromMilliseconds(1000);
  response_count_++;
  if (response_count_ < 20) {
    fetcher_->StartGetRequest(CloudPrintURLFetcher::REQUEST_MAX, url, this,
                              max_retries_, std::string());
  } else {
    // We have already sent 20 requests continuously. And we expect that
    // it takes more than 1 second due to the overload protection settings.
    EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
    std::move(quit_run_loop_).Run();
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcherRetryBackoffTest::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  response_count_++;
  // First attempt + 11 retries = 12 total responses.
  EXPECT_LE(response_count_, 12);
  return CloudPrintURLFetcher::RETRY_REQUEST;
}

void CloudPrintURLFetcherRetryBackoffTest::OnRequestGiveUp() {
  // It takes more than 200 ms to finish all 11 requests.
  EXPECT_TRUE(Time::Now() - start_time_ >= TimeDelta::FromMilliseconds(200));
  std::move(quit_run_loop_).Run();
}

TEST_F(CloudPrintURLFetcherBasicTest, HandleRawResponse) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());
  SetHandleRawResponse(true);

  CreateFetcher(test_server.GetURL("/echo"), 0);
  run_loop_.Run();
}

TEST_F(CloudPrintURLFetcherBasicTest, HandleRawData) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  SetHandleRawData(true);
  CreateFetcher(test_server.GetURL("/echo"), 0);
  run_loop_.Run();
}

TEST_F(CloudPrintURLFetcherOverloadTest, Protect) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("/defaultresponse"));
  CreateFetcher(url, 11);

  run_loop_.Run();
}

TEST_F(CloudPrintURLFetcherRetryBackoffTest, GiveUp) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("/defaultresponse"));
  CreateFetcher(url, 11);

  run_loop_.Run();
}

}  // namespace cloud_print
