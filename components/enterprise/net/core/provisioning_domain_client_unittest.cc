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

class FakeDelegate : public ProvisioningDomainClient::Delegate {
 public:
  net::HttpRequestHeaders extra_headers;

  net::HttpRequestHeaders GetExtraHeaders() const override {
    return extra_headers;
  }
};

class ProvisioningDomainClientTest : public testing::Test {
 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  // Helper method to initiate an HTTP fetch and simulate a network response.
  ProvisioningDomainClientResult FetchWithSimulatedResponse(
      const GURL& url,
      ProvisioningDomainClient::Delegate* delegate,
      const std::optional<std::string>& access_token,
      const std::string& response_body,
      net::HttpStatusCode status_code = net::HTTP_OK) {
    ProvisioningDomainClient client(url, delegate ? delegate : &fake_delegate_,
                                    GetURLLoaderFactory());
    base::test::TestFuture<ProvisioningDomainClientResult> future;

    client.Fetch(access_token, future.GetCallback());

    if (url.is_valid() && test_url_loader_factory_.NumPending() > 0) {
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          url.spec(), response_body, status_code);
    }

    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeDelegate fake_delegate_;
};

TEST_F(ProvisioningDomainClientTest, SuccessfulFetchWithHeaders) {
  base::HistogramTester histogram_tester;
  FakeDelegate delegate;
  delegate.extra_headers.SetHeader(kTestCustomHeaderKey,
                                   kTestCustomHeaderValue);

  ProvisioningDomainClient client(GURL(kTestUrl1), &delegate,
                                  GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch("token123", future.GetCallback());

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

  ProvisioningDomainClientResult result = FetchWithSimulatedResponse(
      GURL(kTestUrl1), /*delegate=*/nullptr, /*access_token=*/std::nullopt,
      kTestJsonResponse1, net::HTTP_CREATED);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kTestJsonResponse1, *result);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomainClient.FetchResult",
      ProvisioningDomainClientFetchResult::kSuccess, 1);
}

TEST_F(ProvisioningDomainClientTest, Http404ErrorWithBodyReturnsError) {
  base::HistogramTester histogram_tester;

  ProvisioningDomainClientResult result = FetchWithSimulatedResponse(
      GURL(kTestUrl1), /*delegate=*/nullptr, /*access_token=*/std::nullopt,
      R"({"error": "not_found"})", net::HTTP_NOT_FOUND);

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
  ProvisioningDomainClient client(GURL(kTestUrl1), &fake_delegate_,
                                  GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch(/*access_token=*/std::nullopt, future.GetCallback());

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
  ProvisioningDomainClient client(GURL(kTestUrl1), &fake_delegate_,
                                  GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch(/*access_token=*/std::nullopt, future.GetCallback());

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
  ProvisioningDomainClient client(GURL("invalid_url"), &fake_delegate_,
                                  GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future;
  client.Fetch(/*access_token=*/std::nullopt, future.GetCallback());

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
  ProvisioningDomainClient client(GURL(kTestUrl1), &fake_delegate_,
                                  GetURLLoaderFactory());

  base::test::TestFuture<ProvisioningDomainClientResult> future1;
  base::test::TestFuture<ProvisioningDomainClientResult> future2;

  client.Fetch(/*access_token=*/std::nullopt, future1.GetCallback());

  EXPECT_TRUE(client.is_fetching());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Second fetch while first is in progress queues callback without creating
  // another network loader.
  client.Fetch(/*access_token=*/std::nullopt, future2.GetCallback());

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

}  // namespace
}  // namespace enterprise_net
