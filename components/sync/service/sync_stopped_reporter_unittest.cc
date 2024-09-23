// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_stopped_reporter.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sync/protocol/sync.pb.h"
#include "net/http/http_status_code.h"
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
 public:
  SyncStoppedReporterTest(const SyncStoppedReporterTest&) = delete;
  SyncStoppedReporterTest& operator=(const SyncStoppedReporterTest&) = delete;

 protected:
  SyncStoppedReporterTest() = default;
  ~SyncStoppedReporterTest() override = default;

  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory());
  }

  GURL interception_url(const GURL& url) {
    return SyncStoppedReporter::GetSyncEventURL(url);
  }

  GURL test_url() { return GURL(kTestURL); }

  std::string user_agent() const { return std::string(kTestUserAgent); }

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
                          shared_url_loader_factory());
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
                          shared_url_loader_factory());
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
  SyncStoppedReporter ssr(test_url(), user_agent(),
                          shared_url_loader_factory());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);

  // Ensure the headers are set correctly.
  EXPECT_EQ(
      base::StrCat({kAuthHeaderPrefix, kAuthToken}),
      intercepted_headers.GetHeader(net::HttpRequestHeaders::kAuthorization)
          .value_or(std::string()));
  EXPECT_EQ(user_agent(),
            intercepted_headers.GetHeader(net::HttpRequestHeaders::kUserAgent)
                .value_or(std::string()));

  sync_pb::EventRequest event_request;
  event_request.ParseFromString(intercepted_body);

  EXPECT_EQ(kCacheGuid, event_request.sync_disabled().cache_guid());
  EXPECT_EQ(kBirthday, event_request.sync_disabled().store_birthday());
  EXPECT_EQ(kEventURL, intercepted_url.spec());
}

TEST_F(SyncStoppedReporterTest, HappyCase) {
  base::HistogramTester histogram_tester;
  url_loader_factory()->AddResponse(interception_url(test_url()).spec(), "");
  SyncStoppedReporter ssr(test_url(), user_agent(),
                          shared_url_loader_factory());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  histogram_tester.ExpectUniqueSample("Sync.SyncStoppedURLFetchResponse",
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("Sync.SyncStoppedURLFetchTimedOut", false,
                                      1);
}

TEST_F(SyncStoppedReporterTest, ServerNotFound) {
  base::HistogramTester histogram_tester;
  url_loader_factory()->AddResponse(interception_url(test_url()).spec(), "",
                                    net::HTTP_NOT_FOUND);
  SyncStoppedReporter ssr(test_url(), user_agent(),
                          shared_url_loader_factory());
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  histogram_tester.ExpectUniqueSample("Sync.SyncStoppedURLFetchResponse",
                                      net::ERR_HTTP_RESPONSE_CODE_FAILURE, 1);
  histogram_tester.ExpectTotalCount("Sync.SyncStoppedURLFetchTimedOut", 0);
}

TEST_F(SyncStoppedReporterTest, Timeout) {
  base::HistogramTester histogram_tester;
  // Mock the underlying loop's clock to trigger the timer at will.
  base::ScopedMockTimeMessageLoopTaskRunner mock_main_runner;
  // No TestURLLoaderFactory::AddResponse(), so the request stays pending.

  SyncStoppedReporter ssr(test_url(), user_agent(),
                          shared_url_loader_factory());

  // Begin request.
  ssr.ReportSyncStopped(kAuthToken, kCacheGuid, kBirthday);

  // Trigger the timeout, 30 seconds should be more than enough.
  ASSERT_TRUE(mock_main_runner->HasPendingTask());
  mock_main_runner->FastForwardBy(base::Seconds(30));
  histogram_tester.ExpectUniqueSample("Sync.SyncStoppedURLFetchTimedOut", true,
                                      1);
}

}  // namespace syncer
