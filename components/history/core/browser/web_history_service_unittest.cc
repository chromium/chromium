// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/web_history_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/history/core/browser/history_types.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Return;

namespace history {

namespace {

// A testing request class that allows expected values to be filled in.
class TestRequest : public WebHistoryService::Request {
 public:
  TestRequest(WebHistoryService::CompletionCallback callback,
              int response_code,
              const std::string& response_body)
      : callback_(std::move(callback)),
        response_code_(response_code),
        response_body_(response_body) {}

  TestRequest(const TestRequest&) = delete;
  TestRequest& operator=(const TestRequest&) = delete;

  ~TestRequest() override = default;

  // WebHistoryService::Request overrides
  bool IsPending() const override { return false; }
  int GetResponseCode() const override { return response_code_; }
  const std::string& GetResponseBody() const override { return response_body_; }
  void SetPostData(const std::string& post_data) override {
    post_data_ = post_data;
  }
  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override {
    SetPostData(post_data);
  }
  void SetUserAgent(const std::string&) override {}
  void Start() override {
    bool success = response_code_ < 400;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), this, success));
  }

 private:
  WebHistoryService::CompletionCallback callback_;
  const int response_code_;
  const std::string response_body_;
  std::string post_data_;
};

// A testing web history service that does extra checks and creates a
// TestRequest instead of a normal request.
class TestingWebHistoryService : public WebHistoryService {
 public:
  explicit TestingWebHistoryService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      // NOTE: Simply pass null object for IdentityManager. WebHistoryService's
      // only usage of this object is to fetch access tokens via RequestImpl,
      // and TestWebHistoryService deliberately replaces this flow with
      // TestRequest.
      : WebHistoryService(nullptr, url_loader_factory) {}

  TestingWebHistoryService(const TestingWebHistoryService&) = delete;
  TestingWebHistoryService& operator=(const TestingWebHistoryService&) = delete;

  ~TestingWebHistoryService() override = default;

  // This is sorta an override but override and static don't mix.
  // This function just calls WebHistoryService::ReadResponse.
  static std::optional<base::Value::Dict> ReadResponse(const Request& request);

  void SetResponse(int response_code, const std::string& response_body) {
    response_code_ = response_code;
    response_body_ = response_body;
  }

  const GURL& last_request_url() const { return last_request_url_; }

 protected:
  std::unique_ptr<Request> CreateRequest(
      const GURL& url,
      CompletionCallback callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override {
    last_request_url_ = url;
    return std::make_unique<TestRequest>(std::move(callback), response_code_,
                                         response_body_);
  }

 private:
  int response_code_ = net::HTTP_OK;
  std::string response_body_;
  GURL last_request_url_;
};

std::optional<base::Value::Dict> TestingWebHistoryService::ReadResponse(
    const Request& request) {
  return WebHistoryService::ReadResponse(request);
}

}  // namespace

// A test class used for testing the WebHistoryService class.
class WebHistoryServiceTest : public testing::Test {
 public:
  WebHistoryServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        web_history_service_(test_shared_loader_factory_) {}

  WebHistoryServiceTest(const WebHistoryServiceTest&) = delete;
  WebHistoryServiceTest& operator=(const WebHistoryServiceTest&) = delete;

  ~WebHistoryServiceTest() override = default;

  std::optional<WebHistoryService::QueryHistoryResult> QueryHistorySynchronous(
      std::string_view query,
      const QueryOptions& options) {
    base::RunLoop run_loop;
    std::optional<WebHistoryService::QueryHistoryResult> result;
    std::unique_ptr<WebHistoryService::Request> request =
        web_history_service_.QueryHistory(
            base::UTF8ToUTF16(query), options,
            base::BindLambdaForTesting(
                [&](WebHistoryService::Request*,
                    base::optional_ref<
                        const WebHistoryService::QueryHistoryResult>
                        query_result) {
                  if (query_result) {
                    result.emplace(*query_result);
                  }
                  run_loop.Quit();
                }),
            PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
    run_loop.Run();
    return result;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestingWebHistoryService web_history_service_;
};

TEST_F(WebHistoryServiceTest, VerifyReadResponse) {
  // Test that properly formatted response with good response code returns true
  // as expected.
  auto request = std::make_unique<TestRequest>(base::DoNothing(), net::HTTP_OK,
                                               R"({
  "history_recording_enabled": true
})");
  auto response_value = TestingWebHistoryService::ReadResponse(*request);
  ASSERT_TRUE(response_value);
  bool enabled_value = false;
  if (std::optional<bool> enabled =
          response_value->FindBool("history_recording_enabled")) {
    enabled_value = *enabled;
  }
  EXPECT_TRUE(enabled_value);

  // Test that properly formatted response with good response code returns false
  // as expected.
  auto request2 = std::make_unique<TestRequest>(base::DoNothing(), net::HTTP_OK,
                                                R"({
  "history_recording_enabled": false
})");
  auto response_value2 = TestingWebHistoryService::ReadResponse(*request2);
  ASSERT_TRUE(response_value2);
  enabled_value = true;
  if (std::optional<bool> enabled =
          response_value2->FindBool("history_recording_enabled")) {
    enabled_value = *enabled;
  }
  EXPECT_FALSE(enabled_value);

  // Test that a bad response code returns false.
  auto request3 =
      std::make_unique<TestRequest>(base::DoNothing(), net::HTTP_UNAUTHORIZED,
                                    R"({
  "history_recording_enabled": true
})");
  auto response_value3 = TestingWebHistoryService::ReadResponse(*request3);
  EXPECT_FALSE(response_value3);

  // Test that improperly formatted response returns false.
  // Note: we expect to see a warning when running this test similar to
  //   "Non-JSON response received from history server".
  // This test tests how that situation is handled.
  auto request4 = std::make_unique<TestRequest>(base::DoNothing(), net::HTTP_OK,
                                                R"({
  "history_recording_enabled": not true
})");
  auto response_value4 = TestingWebHistoryService::ReadResponse(*request4);
  EXPECT_FALSE(response_value4);

  // Test that improperly formatted response returns false.
  auto request5 = std::make_unique<TestRequest>(base::DoNothing(), net::HTTP_OK,
                                                R"({
  "history_recording": true
})");
  auto response_value5 = TestingWebHistoryService::ReadResponse(*request5);
  ASSERT_TRUE(response_value5);
  EXPECT_FALSE(response_value5->FindBool("history_recording_enabled"));
}

TEST_F(WebHistoryServiceTest, QueryHistoryValid) {
  // Test a valid response.
  web_history_service_.SetResponse(net::HTTP_OK,
                                   R"({"event":[{"result":[{
  "url":"https://www.google.com/",
  "title":"Google",
  "favicon_url":"https://www.google.com/favicon.ico",
  "id":[{"timestamp_usec":"12345"}],
  "client_id":"id1"
}]}]})");

  std::optional<WebHistoryService::QueryHistoryResult> result =
      QueryHistorySynchronous("google", QueryOptions());

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->events.size());
  EXPECT_EQ("https://www.google.com/", result->events[0].url.spec());
  EXPECT_EQ("Google", result->events[0].title);
  EXPECT_EQ("https://www.google.com/favicon.ico",
            result->events[0].favicon_url.spec());
  EXPECT_EQ(1u, result->events[0].visits.size());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(12.345),
            result->events[0].visits[0].timestamp);
  EXPECT_EQ("id1", result->events[0].visits[0].client_id);
}

TEST_F(WebHistoryServiceTest, QueryHistoryError) {
  web_history_service_.SetResponse(net::HTTP_INTERNAL_SERVER_ERROR, "");

  std::optional<WebHistoryService::QueryHistoryResult> result =
      QueryHistorySynchronous("", QueryOptions());

  EXPECT_FALSE(result.has_value());
}

TEST_F(WebHistoryServiceTest, QueryHistoryMalformedResponse) {
  web_history_service_.SetResponse(net::HTTP_OK, "this is not json");

  std::optional<WebHistoryService::QueryHistoryResult> result =
      QueryHistorySynchronous("", QueryOptions());

  EXPECT_FALSE(result.has_value());
}

TEST_F(WebHistoryServiceTest, QueryHistoryInvalidOrMissingFields) {
  // A response with no "event" list.
  {
    web_history_service_.SetResponse(net::HTTP_OK, R"({})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->events.empty());
  }

  // An event with no "result" list.
  {
    web_history_service_.SetResponse(net::HTTP_OK, R"({"event":[{}]})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->events.empty());
  }

  // A result with no "url".
  {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"event":[{"result":[{}]}]})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->events.empty());
  }

  // A result with no "id" (visit) list.
  {
    web_history_service_.SetResponse(
        net::HTTP_OK,
        R"({"event":[{"result":[{"url":"https://www.google.com/"}]}]})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->events.empty());
  }

  // A visit with no "timestamp_usec".
  {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"event":[{"result":[{
  "url":"https://www.google.com/",
  "id":[{"not_a_timestamp":"12345"}]
}]}]})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->events.empty());
  }

  // A visit with an invalid "timestamp_usec".
  {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"event":[{"result":[{
  "url":"https://www.google.com/",
  "id":[{"timestamp_usec":"not a number"}]
}]}]})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->events.empty());
  }
}

TEST_F(WebHistoryServiceTest, QueryHistoryUrlConstruction) {
  QueryOptions options;
  options.begin_time = base::Time::FromMillisecondsSinceUnixEpoch(12345000);
  options.end_time = base::Time::FromMillisecondsSinceUnixEpoch(67890000);
  options.max_count = 50;

  QueryHistorySynchronous("search term", options);

  const GURL& url = web_history_service_.last_request_url();
  EXPECT_NE(std::string::npos, url.query().find("min=12345000000"));
  // end_time is inclusive, so 1us is subtracted.
  EXPECT_NE(std::string::npos, url.query().find("max=67889999999"));
  EXPECT_NE(std::string::npos, url.query().find("num=50"));
  EXPECT_NE(std::string::npos, url.query().find("q=search+term"));
}

}  // namespace history
