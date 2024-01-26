// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"

namespace credential_provider {

namespace testing {

// Test the WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService method
// used to make various HTTP requests.
// Parameters are:
// 1.  int - 0:  HTTP call succeeds.
//           1:  Fails due to invalid response from server.
//           2:  Fails due to a retryable HTTP error (like 503).
//           3:  Fails due to a non-retryable HTTP error (like 404).
// 2.  int - Number of retries allowed for a HTTP request.
class GcpWinHttpUrlFetcherTest
    : public GlsRunnerTestBase,
      public ::testing::WithParamInterface<std::tuple<int, int>> {};

TEST_P(GcpWinHttpUrlFetcherTest,
       BuildRequestAndFetchResultFromHttpServiceTest) {
  bool valid_response = std::get<0>(GetParam()) == 0;
  bool invalid_response = std::get<0>(GetParam()) == 1;
  bool retryable_error_response = std::get<0>(GetParam()) == 2;
  bool nonretryable_error_response = std::get<0>(GetParam()) == 3;
  int num_retries = std::get<1>(GetParam());

  const int timeout_in_millis = 12000;
  const std::string header1 = "test-header-1";
  const std::string header1_value = "test-value-1";
  const GURL test_url =
      GURL("https://test-service.googleapis.com/v1/testEndpoint");
  const std::string access_token = "test-access-token";

  base::Value::Dict request;
  request.Set("request-str-key", "request-str-value");
  request.Set("request-int-key", 1234);
  base::TimeDelta request_timeout = base::Milliseconds(timeout_in_millis);
  std::optional<base::Value> request_result;

  auto expected_result = base::Value::Dict()
                             .Set("response-str-key", "response-str-value")
                             .Set("response-int-key", 4321);
  std::string expected_response;
  base::JSONWriter::Write(expected_result, &expected_response);

  std::string response;
  if (invalid_response) {
    response = "Invalid json response";
  } else if (retryable_error_response) {
    response =
        "{\n\"error\": {"
        "\"code\": 503,\n"
        "\"message\": \"Service unavailable\","
        "\"status\": \"UNAVAILABLE\"\n}\n}";
  } else if (nonretryable_error_response) {
    response =
        "{\n\"error\": {"
        "\"code\": 403,\n"
        "\"message\": \"The caller does not have permission\","
        "\"status\": \"PERMISSION_DENIED\"\n}\n}";
  } else {
    response = expected_response;
  }

  if (num_retries == 0) {
    fake_http_url_fetcher_factory()->SetFakeResponse(
        test_url, FakeWinHttpUrlFetcher::Headers(), response);
  } else {
    fake_http_url_fetcher_factory()->SetFakeResponseForSpecifiedNumRequests(
        test_url, FakeWinHttpUrlFetcher::Headers(), response, num_retries);
    fake_http_url_fetcher_factory()->SetFakeResponseForSpecifiedNumRequests(
        test_url, FakeWinHttpUrlFetcher::Headers(), expected_response, 1);
  }
  fake_http_url_fetcher_factory()->SetCollectRequestData(true);

  HRESULT hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      test_url, access_token, {{header1, header1_value}}, request,
      request_timeout, num_retries, &request_result);

  if (num_retries == 0) {
    if (invalid_response || retryable_error_response ||
        nonretryable_error_response) {
      ASSERT_TRUE(FAILED(hr));
    } else {
      ASSERT_EQ(S_OK, hr);
      ASSERT_EQ(expected_result, request_result.value());
    }
  } else {
    if (nonretryable_error_response) {
      ASSERT_TRUE(FAILED(hr));
    } else {
      ASSERT_EQ(S_OK, hr);
      ASSERT_EQ(expected_result, request_result.value());
    }
  }

  if (valid_response || nonretryable_error_response) {
    ASSERT_EQ(1UL, fake_http_url_fetcher_factory()->requests_created());
  } else {
    ASSERT_EQ(num_retries + 1UL,
              fake_http_url_fetcher_factory()->requests_created());
  }

  for (size_t idx = 0;
       idx < fake_http_url_fetcher_factory()->requests_created(); ++idx) {
    FakeWinHttpUrlFetcherFactory::RequestData request_data =
        fake_http_url_fetcher_factory()->GetRequestData(idx);

    ASSERT_EQ(timeout_in_millis, request_data.timeout_in_millis);
    ASSERT_EQ(1u, request_data.headers.count("Authorization"));
    ASSERT_NE(std::string::npos,
              request_data.headers.at("Authorization").find(access_token));
    ASSERT_EQ(1u, request_data.headers.count(header1));
    ASSERT_EQ(header1_value, request_data.headers.at(header1));
    std::optional<base::Value> body_value =
        base::JSONReader::Read(request_data.body);
    ASSERT_EQ(request, body_value->GetDict());
  }
}

TEST_P(GcpWinHttpUrlFetcherTest,
       BuildRequestAndFetchResultFromHttpServiceEmptyRequestTest) {
  int num_retries = 0;
  const int timeout_in_millis = 12000;
  const std::string header1 = "test-header-2";
  const std::string header1_value = "test-value-2";
  const GURL test_url = GURL(
      "https://test-service.googleapis.com/v1/testEndpointForEmptyRequest");
  const std::string access_token = "test-access-token";

  // send empty request dictionary
  base::Value::Dict request;

  base::TimeDelta request_timeout = base::Milliseconds(timeout_in_millis);
  std::optional<base::Value> request_result;

  auto expected_result = base::Value::Dict()
                             .Set("response-str-key", "response-str-value")
                             .Set("response-int-key", 4321);
  std::string expected_response;
  base::JSONWriter::Write(expected_result, &expected_response);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      test_url, FakeWinHttpUrlFetcher::Headers(), expected_response);

  fake_http_url_fetcher_factory()->SetCollectRequestData(true);

  HRESULT hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      test_url, access_token, {{header1, header1_value}}, request,
      request_timeout, num_retries, &request_result);

  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(expected_result, request_result.value());
  ASSERT_EQ(1UL, fake_http_url_fetcher_factory()->requests_created());

  for (size_t idx = 0;
       idx < fake_http_url_fetcher_factory()->requests_created(); ++idx) {
    FakeWinHttpUrlFetcherFactory::RequestData request_data =
        fake_http_url_fetcher_factory()->GetRequestData(idx);

    ASSERT_EQ("", request_data.body);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpWinHttpUrlFetcherTest,
                         ::testing::Combine(::testing::Values(0, 1, 2, 3),
                                            ::testing::Values(0, 1, 3)));

}  // namespace testing
}  // namespace credential_provider
