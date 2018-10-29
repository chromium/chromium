// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_networking_host.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace feed {

using testing::ElementsAre;
using network::SharedURLLoaderFactory;
using network::SharedURLLoaderFactoryInfo;
using network::TestURLLoaderFactory;

namespace {

const char kHistogramNetworkRequestStatusCode[] =
    "ContentSuggestions.Feed.Network.RequestStatusCode";

class MockResponseDoneCallback {
 public:
  MockResponseDoneCallback() : has_run(false), code(0) {}

  void Done(int32_t http_code, std::vector<uint8_t> response) {
    EXPECT_FALSE(has_run);
    has_run = true;
    code = http_code;
    response_bytes = std::move(response);
  }

  bool has_run;
  int32_t code;
  std::vector<uint8_t> response_bytes;
};

}  // namespace

class FeedNetworkingHostTest : public testing::Test {
 protected:
  FeedNetworkingHostTest()
      : mock_task_runner_(new base::TestMockTimeTaskRunner(
            base::TestMockTimeTaskRunner::Type::kBoundToThread)) {
    identity_test_env_.MakePrimaryAccountAvailable("example@gmail.com");
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  ~FeedNetworkingHostTest() override {}

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    net_service_ = std::make_unique<FeedNetworkingHost>(
        identity_test_env_.identity_manager(), "dummy_api_key",
        shared_url_loader_factory_);
  }

  FeedNetworkingHost* service() { return net_service_.get(); }

  identity::IdentityTestEnvironment* identity_env() {
    return &identity_test_env_;
  }

  void RunUntilEmpty() { mock_task_runner_->FastForwardUntilNoTasksRemain(); }

  void Respond(const GURL& url,
               const std::string& response_string,
               net::HttpStatusCode code = net::HTTP_OK,
               network::URLLoaderCompletionStatus status =
                   network::URLLoaderCompletionStatus()) {
    network::ResourceResponseHead head;
    if (code >= 0) {
      head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " + base::NumberToString(code));
      status.decoded_body_length = response_string.length();
    }

    test_factory_.AddResponse(url, head, response_string, status);

    RunUntilEmpty();
  }

  void SendRequestAndRespond(const std::string& url_string,
                             const std::string& request_type,
                             const std::string& request_string,
                             const std::string& response_string,
                             net::HttpStatusCode code,
                             network::URLLoaderCompletionStatus status,
                             MockResponseDoneCallback* done_callback) {
    GURL req_url(url_string);
    std::vector<uint8_t> request_body(request_string.begin(),
                                      request_string.end());
    service()->Send(req_url, request_type, request_body,
                    base::BindOnce(&MockResponseDoneCallback::Done,
                                   base::Unretained(done_callback)));

    Respond(req_url, response_string, code, status);
  }

  void SendRequestAndValidateResponse(
      const std::string& url_string,
      const std::string& request_string,
      const std::string& response_string,
      net::HttpStatusCode code,
      network::URLLoaderCompletionStatus status =
          network::URLLoaderCompletionStatus()) {
    MockResponseDoneCallback done_callback;
    SendRequestAndRespond(url_string, "POST", request_string, response_string,
                          code, status, &done_callback);

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.code, code);
    EXPECT_EQ(std::string(done_callback.response_bytes.begin(),
                          done_callback.response_bytes.end()),
              response_string);
  }

  network::TestURLLoaderFactory* test_factory() {
    return &test_factory_;
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  identity::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<FeedNetworkingHost> net_service_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedNetworkingHostTest);
};

TEST_F(FeedNetworkingHostTest, ShouldSendSuccessfullyEmpty) {
  SendRequestAndValidateResponse("http://foobar.com/feed", "", "",
                                 net::HTTP_OK);
}

TEST_F(FeedNetworkingHostTest, ShouldSendSuccessfullySimple) {
  SendRequestAndValidateResponse("http://foobar.com/feed", "?bar=baz&foo=1",
                                 "{key:'val'}", net::HTTP_OK);
}

TEST_F(FeedNetworkingHostTest, ShouldSendSuccessfullyMultipleInflight) {
  MockResponseDoneCallback done_callback1;
  MockResponseDoneCallback done_callback2;
  MockResponseDoneCallback done_callback3;
  base::HistogramTester histogram_tester;

  SendRequestAndRespond("http://foobar.com/feed", "POST", "", "", net::HTTP_OK,
                        network::URLLoaderCompletionStatus(), &done_callback1);
  SendRequestAndRespond("http://foobar.com/foobar", "POST", "", "",
                        net::HTTP_OK, network::URLLoaderCompletionStatus(),
                        &done_callback2);
  SendRequestAndRespond("http://foobar.com/other", "POST", "", "", net::HTTP_OK,
                        network::URLLoaderCompletionStatus(), &done_callback3);
  RunUntilEmpty();

  EXPECT_TRUE(done_callback1.has_run);
  EXPECT_TRUE(done_callback2.has_run);
  EXPECT_TRUE(done_callback3.has_run);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kHistogramNetworkRequestStatusCode),
      ElementsAre(base::Bucket(/*min=*/200, /*count=*/3)));
}

TEST_F(FeedNetworkingHostTest, ShouldSendSuccessfullyDifferentRequestMethods) {
  std::vector<std::string> request_methods({"POST", "GET", "PUT", "PATCH"});
  for (const auto& method : request_methods) {
    MockResponseDoneCallback done_callback;

    SendRequestAndRespond("http://foobar.com/feed", method, "", "",
                          net::HTTP_OK, network::URLLoaderCompletionStatus(),
                          &done_callback);

    RunUntilEmpty();
    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.code, 200);
  }
}

TEST_F(FeedNetworkingHostTest, ShouldReportProtocolErrorCodes) {
  std::vector<net::HttpStatusCode> error_codes(
      {net::HTTP_BAD_REQUEST, net::HTTP_UNAUTHORIZED, net::HTTP_FORBIDDEN,
       net::HTTP_NOT_FOUND, net::HTTP_INTERNAL_SERVER_ERROR,
       net::HTTP_BAD_GATEWAY, net::HTTP_SERVICE_UNAVAILABLE});

  for (const auto& code : error_codes) {
    base::HistogramTester histogram_tester;
    SendRequestAndValidateResponse("http://foobar.com/feed", "?bar=baz&foo=1",
                                   "error_response_data", code);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kHistogramNetworkRequestStatusCode),
        ElementsAre(base::Bucket(/*min=*/code, /*count=*/1)));
  }
}

TEST_F(FeedNetworkingHostTest, ShouldReportNonProtocolErrorCodes) {
  std::vector<int32_t> error_codes(
      {net::ERR_CERT_COMMON_NAME_INVALID, net::ERR_CERT_DATE_INVALID,
       net::ERR_CERT_WEAK_KEY, net::ERR_NAME_RESOLUTION_FAILED});
  for (const auto& code : error_codes) {
    base::HistogramTester histogram_tester;
    MockResponseDoneCallback done_callback;

    SendRequestAndRespond(
        "http://foobar.com/feed", "POST", "", "", net::HTTP_OK,
        network::URLLoaderCompletionStatus(code), &done_callback);

    EXPECT_TRUE(done_callback.has_run);
    EXPECT_EQ(done_callback.code, code);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kHistogramNetworkRequestStatusCode),
        ElementsAre(base::Bucket(/*min=*/code, /*count=*/1)));
  }
}

TEST_F(FeedNetworkingHostTest, ShouldSetHeadersCorrectly) {
  MockResponseDoneCallback done_callback;
  net::HttpRequestHeaders headers;
  base::RunLoop interceptor_run_loop;
  base::HistogramTester histogram_tester;

  test_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        headers = request.headers;
        interceptor_run_loop.Quit();
      }));

  SendRequestAndRespond("http://foobar.com/feed", "POST", "", "",
                      net::HTTP_OK, network::URLLoaderCompletionStatus(),
                      &done_callback);

  std::string content_encoding;
  std::string authorization;
  EXPECT_TRUE(headers.GetHeader("content-encoding", &content_encoding));
  EXPECT_TRUE(headers.GetHeader("Authorization", &authorization));

  EXPECT_EQ(content_encoding, "gzip");
  EXPECT_EQ(authorization, "Bearer access_token");
}

TEST_F(FeedNetworkingHostTest, ShouldReportSizeHistograms) {
  std::string uncompressed_request_string(2048, 'a');
  std::string response_string(1024, 'b');
  base::HistogramTester histogram_tester;

  SendRequestAndValidateResponse("http://foobar.com/feed",
                                 uncompressed_request_string, response_string,
                                 net::HTTP_OK);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "ContentSuggestions.Feed.Network.ResponseSizeKB"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  // A single character repeated 2048 times compresses to well under 1kb.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "ContentSuggestions.Feed.Network.RequestSizeKB.Compressed"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
}

TEST_F(FeedNetworkingHostTest, CancellationIsSafe) {
  MockResponseDoneCallback done_callback;
  MockResponseDoneCallback done_callback2;
  std::vector<uint8_t> request_body;

  service()->Send(GURL("http://foobar.com/feed"), "POST", request_body,
                  base::BindOnce(&MockResponseDoneCallback::Done,
                                 base::Unretained(&done_callback)));

  identity_env()->SetAutomaticIssueOfAccessTokens(false);
  service()->Send(GURL("http://foobar.com/feed2"), "POST", request_body,
                  base::BindOnce(&MockResponseDoneCallback::Done,
                                 base::Unretained(&done_callback2)));
  RunUntilEmpty();
  service()->CancelRequests();
}

TEST_F(FeedNetworkingHostTest, ShouldIncludeAPIKeyForAuthError) {
  identity_env()->SetAutomaticIssueOfAccessTokens(false);
  MockResponseDoneCallback done_callback;
  base::HistogramTester histogram_tester;

  service()->Send(GURL("http://foobar.com/feed"), "POST",
                  std::vector<uint8_t>(),
                  base::BindOnce(&MockResponseDoneCallback::Done,
                                 base::Unretained(&done_callback)));
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  Respond(GURL("http://foobar.com/feed?key=dummy_api_key"), "");
  EXPECT_TRUE(done_callback.has_run);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "ContentSuggestions.Feed.Network.TokenFetchStatus"),
      ElementsAre(base::Bucket(
          /*min=*/GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS,
          /*count=*/1)));
}

// Disabled for chromeos, which doesn't allow for there not to be a signed in
// user.
#if !defined(OS_CHROMEOS)
TEST_F(FeedNetworkingHostTest, ShouldIncludeAPIKeyForNoSignedInUser) {
  identity_env()->ClearPrimaryAccount();
  MockResponseDoneCallback done_callback;

  service()->Send(GURL("http://foobar.com/feed"), "POST",
                  std::vector<uint8_t>(),
                  base::BindOnce(&MockResponseDoneCallback::Done,
                                 base::Unretained(&done_callback)));

  Respond(GURL("http://foobar.com/feed?key=dummy_api_key"), "");
  EXPECT_TRUE(done_callback.has_run);
}
#endif

}  // namespace feed
