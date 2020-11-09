// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_stopped_reporter.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "components/sync/protocol/sync.pb.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

const char kTestURL[] = "http://chromium.org/test";
const char kTestURLTrailingSlash[] = "http://chromium.org/test/";
const char kEventURL[] = "http://chromium.org/test/event";

const char kTestUserAgent[] = "the_fifth_element";
const char kAuthToken[] = "multipass";
const char kCacheGuid[] = "leeloo";
const char kBirthday[] = "2263";

const char kAuthHeaderPrefix[] = "Bearer ";

class SyncStoppedReporterTest : public testing::Test {
 protected:
  SyncStoppedReporterTest() {}
  ~SyncStoppedReporterTest() override {}

  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory());
  }

  void RequestFinishedCallback(const SyncStoppedReporter::Result& result) {
    request_result_ = result;
  }

  GURL interception_url(const GURL& url) {
    return SyncStoppedReporter::GetSyncEventURL(url);
  }

  GURL test_url() { return GURL(kTestURL); }

  void call_on_timeout(SyncStoppedReporter* ssr) { ssr->OnTimeout(); }

  std::string user_agent() const { return std::string(kTestUserAgent); }

  SyncStoppedReporter::ResultCallback callback() {
    return base::BindOnce(&SyncStoppedReporterTest::RequestFinishedCallback,
                          base::Unretained(this));
  }

  const SyncStoppedReporter::Result& request_result() const {
    return request_result_;
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &test_url_loader_factory_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return test_shared_loader_factory_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  SyncStoppedReporter::Result request_result_;

  DISALLOW_COPY_AND_ASSIGN(SyncStoppedReporterTest);
};

// Test that the event URL gets constructed correctly.
TEST_F(SyncStoppedReporterTest, EventURL) {
  GURL intercepted_url;
  url_loader_factory()->AddResponse(interception_url(GURL(kTestURL)).spec(),
                                    "");
  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_url = request.url;
      }));
  SyncStoppedReporter ssr(GURL(kTestURL), user_agent(),
                          shared_url_loader_factory(), callback());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);
  EXPECT_EQ(kEventURL, intercepted_url.spec());
}

// Test that the event URL gets constructed correctly with a trailing slash.
TEST_F(SyncStoppedReporterTest, EventURLWithSlash) {
  GURL intercepted_url;
  url_loader_factory()->AddResponse(
      interception_url(GURL(kTestURLTrailingSlash)).spec(), "");
  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_url = request.url;
      }));
  SyncStoppedReporter ssr(GURL(kTestURLTrailingSlash), user_agent(),
                          shared_url_loader_factory(), callback());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);
  EXPECT_EQ(kEventURL, intercepted_url.spec());
}

// Test that the URLFetcher gets configured correctly.
TEST_F(SyncStoppedReporterTest, FetcherConfiguration) {
  GURL intercepted_url;
  net::HttpRequestHeaders intercepted_headers;
  std::string intercepted_body;
  url_loader_factory()->AddResponse(interception_url(test_url()).spec(), "");
  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_url = request.url;
        intercepted_headers = request.headers;
        intercepted_body = network::GetUploadData(request);
      }));
  SyncStoppedReporter ssr(test_url(), user_agent(), shared_url_loader_factory(),
                          callback());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);

  // Ensure the headers are set correctly.
  std::string header;
  intercepted_headers.GetHeader(net::HttpRequestHeaders::kAuthorization,
                                &header);
  std::string auth_header(kAuthHeaderPrefix);
  auth_header.append(kAuthToken);
  EXPECT_EQ(auth_header, header);
  intercepted_headers.GetHeader(net::HttpRequestHeaders::kUserAgent, &header);
  EXPECT_EQ(user_agent(), header);

  sync_pb::EventRequest event_request;
  event_request.ParseFromString(intercepted_body);

  EXPECT_EQ(kCacheGuid, event_request.sync_disabled().cache_guid());
  EXPECT_EQ(kBirthday, event_request.sync_disabled().store_birthday());
  EXPECT_EQ(kEventURL, intercepted_url.spec());
}

TEST_F(SyncStoppedReporterTest, HappyCase) {
  url_loader_factory()->AddResponse(interception_url(test_url()).spec(), "");
  SyncStoppedReporter ssr(test_url(), user_agent(), shared_url_loader_factory(),
                          callback());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(SyncStoppedReporter::RESULT_SUCCESS, request_result());
}

TEST_F(SyncStoppedReporterTest, ServerNotFound) {
  url_loader_factory()->AddResponse(interception_url(test_url()).spec(), "",
                                    net::HTTP_NOT_FOUND);
  SyncStoppedReporter ssr(test_url(), user_agent(), shared_url_loader_factory(),
                          callback());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(SyncStoppedReporter::RESULT_ERROR, request_result());
}

TEST_F(SyncStoppedReporterTest, Timeout) {
  url_loader_factory()->AddResponse(interception_url(test_url()).spec(), "");
  // Mock the underlying loop's clock to trigger the timer at will.
  base::ScopedMockTimeMessageLoopTaskRunner mock_main_runner;

  SyncStoppedReporter ssr(test_url(), user_agent(), shared_url_loader_factory(),
                          callback());

  // Begin request.
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);

  // Trigger the timeout.
  ASSERT_TRUE(mock_main_runner->HasPendingTask());
  call_on_timeout(&ssr);
  mock_main_runner->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(SyncStoppedReporter::RESULT_TIMEOUT, request_result());
}

TEST_F(SyncStoppedReporterTest, NoCallback) {
  url_loader_factory()->AddResponse(interception_url(GURL(kTestURL)).spec(),
                                    "");
  SyncStoppedReporter ssr(GURL(kTestURL), user_agent(),
                          shared_url_loader_factory(),
                          SyncStoppedReporter::ResultCallback());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(SyncStoppedReporterTest, NoCallbackTimeout) {
  url_loader_factory()->AddResponse(interception_url(GURL(kTestURL)).spec(),
                                    "");
  // Mock the underlying loop's clock to trigger the timer at will.
  base::ScopedMockTimeMessageLoopTaskRunner mock_main_runner;

  SyncStoppedReporter ssr(GURL(kTestURL), user_agent(),
                          shared_url_loader_factory(),
                          SyncStoppedReporter::ResultCallback());

  // Begin request.
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);

  // Trigger the timeout.
  ASSERT_TRUE(mock_main_runner->HasPendingTask());
  call_on_timeout(&ssr);
  mock_main_runner->FastForwardUntilNoTasksRemain();
}

}  // namespace syncer
