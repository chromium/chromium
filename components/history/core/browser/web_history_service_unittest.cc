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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/history/core/browser/features.h"
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

  void SetResponse(int response_code, const std::string& response_body) {
    response_code_ = response_code;
    response_body_ = response_body;
  }

  const GURL& last_request_url() const { return last_request_url_; }

  using WebHistoryService::server_version_info_for_test;

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

}  // namespace

// A test class used for testing the WebHistoryService class.
class WebHistoryServiceTest : public testing::TestWithParam<bool> {
 public:
  WebHistoryServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        web_history_service_(test_shared_loader_factory_) {
    features_.InitWithFeatureState(kWebHistoryUseNewApi, IsNewAPIEnabled());
  }

  WebHistoryServiceTest(const WebHistoryServiceTest&) = delete;
  WebHistoryServiceTest& operator=(const WebHistoryServiceTest&) = delete;

  ~WebHistoryServiceTest() override = default;

  bool IsNewAPIEnabled() const { return GetParam(); }

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

  bool QueryWebAndAppActivitySynchronous() {
    base::RunLoop run_loop;
    bool result = false;
    web_history_service_.QueryWebAndAppActivity(
        base::BindLambdaForTesting([&](bool success) {
          result = success;
          run_loop.Quit();
        }),
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
    run_loop.Run();
    return result;
  }

  bool ExpireHistorySynchronous(
      const std::vector<ExpireHistoryArgs>& expire_list) {
    base::RunLoop run_loop;
    bool result = false;
    web_history_service_.ExpireHistory(
        expire_list, base::BindLambdaForTesting([&](bool success) {
          result = success;
          run_loop.Quit();
        }),
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
    run_loop.Run();
    return result;
  }

 protected:
  base::test::ScopedFeatureList features_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestingWebHistoryService web_history_service_;
};

INSTANTIATE_TEST_SUITE_P(,
                         WebHistoryServiceTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NewAPI" : "OldAPI";
                         });

TEST_P(WebHistoryServiceTest, QueryHistoryValid) {
  // Test a valid response.
  if (IsNewAPIEnabled()) {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"lookup":[{"chromeHistory":[{
  "url":"https://www.google.com/",
  "timestamp":"12345",
  "title":"Google",
  "faviconUrl":"https://www.google.com/favicon.ico",
  "clientId":"id1"
}]}]})");
  } else {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"event":[{"result":[{
  "url":"https://www.google.com/",
  "title":"Google",
  "favicon_url":"https://www.google.com/favicon.ico",
  "id":[{"timestamp_usec":"12345", "client_id":"id1"}]
}]}]})");
  }

  std::optional<WebHistoryService::QueryHistoryResult> result =
      QueryHistorySynchronous("google", QueryOptions());

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->visits.size());
  EXPECT_EQ("https://www.google.com/", result->visits[0].url.spec());
  EXPECT_EQ("Google", result->visits[0].title);
  EXPECT_EQ("https://www.google.com/favicon.ico",
            result->visits[0].favicon_url.spec());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(12.345),
            result->visits[0].timestamp);
  EXPECT_EQ("id1", result->visits[0].client_id);
}

TEST_P(WebHistoryServiceTest, QueryHistoryError) {
  web_history_service_.SetResponse(net::HTTP_INTERNAL_SERVER_ERROR, "");

  std::optional<WebHistoryService::QueryHistoryResult> result =
      QueryHistorySynchronous("", QueryOptions());

  EXPECT_FALSE(result.has_value());
}

TEST_P(WebHistoryServiceTest, QueryHistoryMalformedResponse) {
  web_history_service_.SetResponse(net::HTTP_OK, "this is not json");

  std::optional<WebHistoryService::QueryHistoryResult> result =
      QueryHistorySynchronous("", QueryOptions());

  EXPECT_FALSE(result.has_value());
}

TEST_P(WebHistoryServiceTest, QueryHistoryInvalidOrMissingFields) {
  // A response with no "event"/"lookup" list.
  {
    web_history_service_.SetResponse(net::HTTP_OK, R"({})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->visits.empty());
  }

  // An event with no "result"/"chromeHistory" list.
  {
    if (IsNewAPIEnabled()) {
      web_history_service_.SetResponse(net::HTTP_OK, R"({"lookup":[{}]})");
    } else {
      web_history_service_.SetResponse(net::HTTP_OK, R"({"event":[{}]})");
    }

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->visits.empty());
  }

  // An empty result.
  {
    if (IsNewAPIEnabled()) {
      web_history_service_.SetResponse(
          net::HTTP_OK, R"({"lookup":[{"chromeHistory":[{}]}]})");
    } else {
      web_history_service_.SetResponse(net::HTTP_OK,
                                       R"({"event":[{"result":[{}]}]})");
    }

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->visits.empty());
  }

  // A result with no "id" (visit) list.
  if (!IsNewAPIEnabled()) {
    web_history_service_.SetResponse(
        net::HTTP_OK,
        R"({"event":[{"result":[{"url":"https://www.google.com/"}]}]})");

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->visits.empty());
  }

  // A visit with no timestamp.
  {
    if (IsNewAPIEnabled()) {
      web_history_service_.SetResponse(net::HTTP_OK,
                                       R"({"lookup":[{"chromeHistory":[{
  "url":"https://www.google.com/",
  "not_a_timestamp":"12345"
}]}]})");
    } else {
      web_history_service_.SetResponse(net::HTTP_OK,
                                       R"({"event":[{"result":[{
  "url":"https://www.google.com/",
  "id":[{"not_a_timestamp":"12345"}]
}]}]})");
    }

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->visits.empty());
  }

  // A visit with an invalid timestamp.
  {
    if (IsNewAPIEnabled()) {
      web_history_service_.SetResponse(net::HTTP_OK,
                                       R"({"event":[{"result":[{
  "url":"https://www.google.com/",
  "timestamp":"not a number"
}]}]})");
    } else {
      web_history_service_.SetResponse(net::HTTP_OK,
                                       R"({"event":[{"result":[{
  "url":"https://www.google.com/",
  "id":[{"timestamp_usec":"not a number"}]
}]}]})");
    }

    std::optional<WebHistoryService::QueryHistoryResult> result =
        QueryHistorySynchronous("", QueryOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->visits.empty());
  }
}

TEST_P(WebHistoryServiceTest, QueryHistoryUrlConstruction) {
  QueryOptions options;
  options.begin_time = base::Time::FromMillisecondsSinceUnixEpoch(12345000);
  options.end_time = base::Time::FromMillisecondsSinceUnixEpoch(67890000);
  options.max_count = 50;

  QueryHistorySynchronous("search term", options);

  const GURL& url = web_history_service_.last_request_url();
  if (IsNewAPIEnabled()) {
    EXPECT_TRUE(url.query().empty());
  } else {
    EXPECT_NE(std::string::npos, url.query().find("min=12345000000"));
    // end_time is inclusive, so 1us is subtracted.
    EXPECT_NE(std::string::npos, url.query().find("max=67889999999"));
    EXPECT_NE(std::string::npos, url.query().find("num=50"));
    EXPECT_NE(std::string::npos, url.query().find("q=search+term"));
  }
}

TEST_P(WebHistoryServiceTest, QueryWebAndAppActivityEnabled) {
  // Test a valid response that says WAA is enabled.
  if (IsNewAPIEnabled()) {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"facsSetting":[{
  "dataRecordingEnabled": true
}]})");
  } else {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"history_recording_enabled": true})");
  }

  EXPECT_TRUE(QueryWebAndAppActivitySynchronous());
}

TEST_P(WebHistoryServiceTest, QueryWebAndAppActivityDisabled) {
  // Test a valid response that says WAA is disabled.
  if (IsNewAPIEnabled()) {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"facsSetting":[{
  "dataRecordingEnabled": false
}]})");
  } else {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"history_recording_enabled": false})");
  }

  EXPECT_FALSE(QueryWebAndAppActivitySynchronous());
}

TEST_P(WebHistoryServiceTest, QueryWebAndAppActivityError) {
  web_history_service_.SetResponse(net::HTTP_INTERNAL_SERVER_ERROR, "");

  EXPECT_FALSE(QueryWebAndAppActivitySynchronous());
}

TEST_P(WebHistoryServiceTest, QueryWebAndAppActivityMalformedResponse) {
  web_history_service_.SetResponse(net::HTTP_OK, "this is not json");

  EXPECT_FALSE(QueryWebAndAppActivitySynchronous());
}

TEST_P(WebHistoryServiceTest, QueryWebAndAppActivityMisnamedField) {
  // Test a response that contains differently-named response fields.
  if (IsNewAPIEnabled()) {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"facsSetting":[{
  "dataRecording": true
}]})");
  } else {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"history_recording": true})");
  }

  EXPECT_FALSE(QueryWebAndAppActivitySynchronous());
}

TEST_P(WebHistoryServiceTest, ExpireHistoryValid) {
  if (IsNewAPIEnabled()) {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"versionInfo": "some_token"})");
  } else {
    web_history_service_.SetResponse(net::HTTP_OK,
                                     R"({"version_info": "some_token"})");
  }

  EXPECT_TRUE(ExpireHistorySynchronous({}));
  EXPECT_EQ(web_history_service_.server_version_info_for_test(), "some_token");
}

TEST_P(WebHistoryServiceTest, ExpireHistoryValidNoVersionInfo) {
  web_history_service_.SetResponse(net::HTTP_OK, R"({})");

  // Version info in the response is optional, so this should still succeed.
  EXPECT_TRUE(ExpireHistorySynchronous({}));
  EXPECT_TRUE(web_history_service_.server_version_info_for_test().empty());
}

TEST_P(WebHistoryServiceTest, ExpireHistoryError) {
  web_history_service_.SetResponse(net::HTTP_INTERNAL_SERVER_ERROR, "");

  EXPECT_FALSE(ExpireHistorySynchronous({}));
}

TEST_P(WebHistoryServiceTest, ExpireHistoryMalformedResponse) {
  web_history_service_.SetResponse(net::HTTP_OK, "this is not json");

  EXPECT_FALSE(ExpireHistorySynchronous({}));
}

}  // namespace history
