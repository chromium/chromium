// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/provisioning_domain_client.h"

#include <optional>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

namespace {

constexpr char kTestUrl1[] = "https://example.com/.well-known/pvd";
constexpr char kTestAuthHeaderValue[] = "Bearer token123";
constexpr char kTestCustomHeaderKey[] = "x-custom-header";
constexpr char kTestCustomHeaderValue[] = "custom-val";
constexpr char kTestJsonResponse1[] = R"({"identifier": "example.com"})";

class ProvisioningDomainClientTest : public testing::Test {
 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  // Helper method to initiate an HTTP fetch and simulate a network response.
  ProvisioningDomainClientResult FetchWithSimulatedResponse(
      ProvisioningDomainClient& client,
      const GURL& url,
      net::HttpRequestHeaders headers,
      const std::string& response_body,
      net::HttpStatusCode status_code = net::HTTP_OK) {
    base::test::TestFuture<ProvisioningDomainClientResult> future;

    client.Fetch(url, std::move(headers), future.GetCallback());

    if (url.is_valid() && test_url_loader_factory_.NumPending() > 0) {
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          url.spec(), response_body, status_code);
    }

    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(ProvisioningDomainClientTest, SuccessfulFetchWithHeaders) {
  base::HistogramTester histogram_tester;
  ProvisioningDomainClient client(GetURLLoaderFactory());

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                    kTestAuthHeaderValue);
  headers.SetHeader(kTestCustomHeaderKey, kTestCustomHeaderValue);

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch(GURL(kTestUrl1), headers, future.GetCallback());

  EXPECT_TRUE(client.is_fetching());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  const network::ResourceRequest& pending_req =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(kTestUrl1, pending_req.url.spec());
  EXPECT_TRUE(pending_req.load_flags & net::LOAD_BYPASS_PROXY);
  EXPECT_EQ(
      kTestAuthHeaderValue,
      pending_req.headers.GetHeader(net::HttpRequestHeaders::kAuthorization)
          .value_or(""));
  EXPECT_EQ(kTestCustomHeaderValue,
            pending_req.headers.GetHeader(kTestCustomHeaderKey).value_or(""));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestUrl1, kTestJsonResponse1, net::HTTP_OK);

  ProvisioningDomainClientResult result = future.Take();
  EXPECT_FALSE(client.is_fetching());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kTestJsonResponse1, *result);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kSuccess, 1);
}

TEST_F(ProvisioningDomainClientTest, Http201CreatedReturnsSuccess) {
  base::HistogramTester histogram_tester;
  ProvisioningDomainClient client(GetURLLoaderFactory());

  ProvisioningDomainClientResult result = FetchWithSimulatedResponse(
      client, GURL(kTestUrl1), net::HttpRequestHeaders(), kTestJsonResponse1,
      net::HTTP_CREATED);

  EXPECT_FALSE(client.is_fetching());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kTestJsonResponse1, *result);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kSuccess, 1);
}

TEST_F(ProvisioningDomainClientTest, Http404ErrorWithBodyReturnsError) {
  base::HistogramTester histogram_tester;
  ProvisioningDomainClient client(GetURLLoaderFactory());

  ProvisioningDomainClientResult result = FetchWithSimulatedResponse(
      client, GURL(kTestUrl1), net::HttpRequestHeaders(),
      R"({"error": "not_found"})", net::HTTP_NOT_FOUND);

  EXPECT_FALSE(client.is_fetching());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(net::HTTP_NOT_FOUND, result.error().response_code);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kHttpError, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.HttpResponseCode", 404, 1);
}

TEST_F(ProvisioningDomainClientTest, NetworkErrorReturnsNetError) {
  base::HistogramTester histogram_tester;
  ProvisioningDomainClient client(GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch(GURL(kTestUrl1), net::HttpRequestHeaders(),
               future.GetCallback());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  network::URLLoaderCompletionStatus status(net::ERR_CONNECTION_FAILED);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kTestUrl1), status, network::mojom::URLResponseHead::New(), "");

  ProvisioningDomainClientResult result = future.Take();
  EXPECT_FALSE(client.is_fetching());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(net::ERR_CONNECTION_FAILED, result.error().net_error);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kNetError, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.NetError",
      -net::ERR_CONNECTION_FAILED, 1);
}

TEST_F(ProvisioningDomainClientTest, Http500RetriesAndSucceeds) {
  base::HistogramTester histogram_tester;
  ProvisioningDomainClient client(GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch(GURL(kTestUrl1), net::HttpRequestHeaders(),
               future.GetCallback());

  // Simulate 500 Internal Server Error for initial request -> triggers retry.
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestUrl1, "", net::HTTP_INTERNAL_SERVER_ERROR);

  // Retry request is queued in test factory; simulate 200 OK for retry attempt.
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestUrl1, kTestJsonResponse1, net::HTTP_OK);

  ProvisioningDomainClientResult result = future.Take();
  EXPECT_FALSE(client.is_fetching());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kTestJsonResponse1, *result);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kSuccess, 1);
}

TEST_F(ProvisioningDomainClientTest, InvalidUrlReturnsErrorImmediately) {
  base::HistogramTester histogram_tester;
  ProvisioningDomainClient client(GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch(GURL("invalid_url"), net::HttpRequestHeaders(),
               future.GetCallback());

  ProvisioningDomainClientResult result = future.Take();
  EXPECT_FALSE(client.is_fetching());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(net::ERR_INVALID_URL, result.error().net_error);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kNetError, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.NetError", -net::ERR_INVALID_URL, 1);
}

TEST_F(ProvisioningDomainClientTest, ConcurrentFetchesReuseInFlightRequest) {
  ProvisioningDomainClient client(GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future1;
  base::test::TestFuture<ProvisioningDomainClientResult> future2;

  client.Fetch(GURL(kTestUrl1), net::HttpRequestHeaders(),
               future1.GetCallback());

  EXPECT_TRUE(client.is_fetching());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Second fetch while first is in progress queues callback without creating
  // another network loader.
  client.Fetch(GURL(kTestUrl1), net::HttpRequestHeaders(),
               future2.GetCallback());

  EXPECT_TRUE(client.is_fetching());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestUrl1, kTestJsonResponse1, net::HTTP_OK);

  ProvisioningDomainClientResult result1 = future1.Take();
  ProvisioningDomainClientResult result2 = future2.Take();

  EXPECT_FALSE(client.is_fetching());
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(kTestJsonResponse1, *result1);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(kTestJsonResponse1, *result2);
}

TEST_F(ProvisioningDomainClientTest, MismatchedUrlFetchReturnsError) {
  ProvisioningDomainClient client(GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future1;
  base::test::TestFuture<ProvisioningDomainClientResult> future2;

  client.Fetch(GURL(kTestUrl1), net::HttpRequestHeaders(),
               future1.GetCallback());

  EXPECT_TRUE(client.is_fetching());

  // Second fetch with a different URL while a fetch is in progress should fail.
  client.Fetch(GURL("https://different.example.com/pvd"),
               net::HttpRequestHeaders(), future2.GetCallback());

  ProvisioningDomainClientResult result2 = future2.Take();
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(net::ERR_FAILED, result2.error().net_error);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestUrl1, kTestJsonResponse1, net::HTTP_OK);

  ProvisioningDomainClientResult result1 = future1.Take();
  EXPECT_FALSE(client.is_fetching());
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(kTestJsonResponse1, *result1);
}

}  // namespace
}  // namespace enterprise_net
