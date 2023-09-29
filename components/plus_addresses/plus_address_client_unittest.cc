// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace plus_addresses {
namespace {
std::string LatencyHistogramFor(PlusAddressNetworkRequestType type) {
  return base::ReplaceStringPlaceholders(
      "Autofill.PlusAddresses.NetworkRequest.$1.Latency",
      {PlusAddressMetrics::PlusAddressNetworkRequestTypeToString(type)},
      /*offsets=*/nullptr);
}
std::string ResponseCodeHistogramFor(PlusAddressNetworkRequestType type) {
  return base::ReplaceStringPlaceholders(
      "Autofill.PlusAddresses.NetworkRequest.$1.ResponseCode",
      {PlusAddressMetrics::PlusAddressNetworkRequestTypeToString(type)},
      /*offsets=*/nullptr);
}
std::string ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType type) {
  return base::ReplaceStringPlaceholders(
      "Autofill.PlusAddresses.NetworkRequest.$1.ResponseByteSize",
      {PlusAddressMetrics::PlusAddressNetworkRequestTypeToString(type)},
      /*offsets=*/nullptr);
}
constexpr char kPlusAddressOauthErrorHistogram[] =
    "Autofill.PlusAddresses.NetworkRequest.OauthError";
}  // namespace

// Tests that use fake out the URL loading and issues requests to the enterprise
// provided server.
class PlusAddressClientRequests : public ::testing::Test {
 public:
  PlusAddressClientRequests()
      : scoped_shared_url_loader_factory(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory)),
        identity_manager(identity_test_env.identity_manager()) {
    test_url_loader_factory.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request = request;
        }));
    features_.InitAndEnableFeatureWithParameters(
        kFeature, {{kEnterprisePlusAddressServerUrl.name, server_base_url},
                   {kEnterprisePlusAddressOAuthScope.name, test_scope}});
    clock_.SetNow(start_time);
  }

 protected:
  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  base::Clock* test_clock() { return &clock_; }

  // Not used directly, but required for `IdentityTestEnvironment` to work.
  base::test::TaskEnvironment task_environment;
  std::string server_base_url = "https://enterprise.foo/";
  std::string test_scope = "scope";
  std::string fullProfileEndpoint =
      base::StrCat({server_base_url, kServerPlusProfileEndpoint});
  std::string fullReserveEndpoint =
      base::StrCat({server_base_url, kServerReservePlusAddressEndpoint});
  std::string fullConfirmEndpoint =
      base::StrCat({server_base_url, kServerCreatePlusAddressEndpoint});
  std::string token = "myToken";
  signin::AccessTokenInfo eternal_token_info =
      signin::AccessTokenInfo(token, base::Time::Max(), "");

  // Issue all requests starting at this time to test the latency metrics.
  base::Time start_time = base::Time::FromDoubleT(1);

  scoped_refptr<network::SharedURLLoaderFactory>
      scoped_shared_url_loader_factory;
  network::TestURLLoaderFactory test_url_loader_factory;
  network::ResourceRequest last_request;

  signin::IdentityTestEnvironment identity_test_env;
  raw_ptr<signin::IdentityManager> identity_manager;

  base::HistogramTester histogram_tester;

 private:
  base::test::ScopedFeatureList features_;
  base::SimpleTestClock clock_;
  data_decoder::test::InProcessDataDecoder decoder_;
};

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, CreatePlusAddressV1_IssuesCorrectRequest) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  std::string site = "https://foobar.com";
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.CreatePlusAddress(site, base::DoNothing());

  // Validate that the V1 Create request uses the right url and requests method.
  EXPECT_EQ(last_request.url, fullProfileEndpoint);
  EXPECT_EQ(last_request.method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes "myToken".
  std::string authorization_value;
  last_request.headers.GetHeader("Authorization", &authorization_value);
  EXPECT_EQ(authorization_value, "Bearer " + token);

  // Validate the request payload.
  ASSERT_NE(last_request.request_body, nullptr);
  ASSERT_EQ(last_request.request_body->elements()->size(), 1u);
  absl::optional<base::Value> body =
      base::JSONReader::Read(last_request.request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  ASSERT_TRUE(body.has_value() && body->is_dict());
  std::string* facet_entry = body->GetDict().FindString("facet");
  ASSERT_NE(facet_entry, nullptr);
  EXPECT_EQ(*facet_entry, site);
}

TEST_F(PlusAddressClientRequests,
       CreatePlusAddressV1_EnqueuedUntilOAuthTokenFetched) {
  identity_test_env.MakePrimaryAccountAvailable("foo@plus.plus",
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  std::string site = "https://foobar.com";
  base::MockOnceCallback<void(const std::string&)> callback;
  // Make a request when the PlusAddressClient has an expired OAuth token.
  EXPECT_CALL(callback, Run).Times(0);
  client.CreatePlusAddress(site, callback.Get());

  // Verify that CreatePlusAddress hasn't already sent the network request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 0);

  // CreatePlusAddress will  run `callback` after an OAuth token is retrieved.
  EXPECT_CALL(callback, Run).Times(1);
  identity_test_env
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {test_scope});

  // Unblock the pending request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"(
    {
      "plusProfile": {
          "unwanted": 123,
          "facet": "youtube.com",
          "plusEmail" : {
            "plusAddress": "plusone@plus.plus"
          }
        },
      "unwanted": "abc"
    }
    )");
}

// For tests that cover successful but unexpected server responses, see the
// PlusAddressParsing.FromV1Create tests.
TEST_F(PlusAddressClientRequests, CreatePlusAddressV1_RunsCallbackOnSuccess) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());
  std::string site = "https://foobar.com";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.CreatePlusAddress(site, on_response_parsed.Get());
  // Fulfill the request and the callback should be run
  EXPECT_CALL(on_response_parsed, Run("plusone@plus.plus")).Times(1);

  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  const std::string json = R"(
    {
      "plusProfile": {
          "unwanted": 123,
          "facet": "youtube.com",
          "plusEmail" : {
            "plusAddress": "plusone@plus.plus"
          }
        },
      "unwanted": "abc"
    }
    )";
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            json);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kGetOrCreate), latency,
      1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kGetOrCreate),
      200, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kGetOrCreate),
      json.size(), 1);
}

// TODO (kaklilu): Add tests verifying behavior when request times out or the
// response is too large to be downloaded.
TEST_F(PlusAddressClientRequests,
       CreatePlusAddressV1_FailedRequestDoesntRunCallback) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());
  std::string site = "https://foobar.com";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.CreatePlusAddress(site, on_response_parsed.Get());

  // The request fails and the callback is never run
  EXPECT_CALL(on_response_parsed, Run).Times(0);
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      fullProfileEndpoint, "", net::HTTP_NOT_FOUND));
  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kGetOrCreate), latency,
      1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kGetOrCreate),
      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectTotalCount(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kGetOrCreate),
      0);
}

TEST_F(PlusAddressClientRequests,
       CreatePlusAddressV1_HandlesConcurrentRequests) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);

  base::MockOnceCallback<void(const std::string&)> first_request;
  base::MockOnceCallback<void(const std::string&)> second_request;
  // Send two requests in quick succession
  client.CreatePlusAddress("hulu.com", first_request.Get());
  client.CreatePlusAddress("netflix.com", second_request.Get());

  // The first callback should be run once the server responds to its request.
  PlusAddressMap expected;
  EXPECT_CALL(first_request, Run("plusthree@plus.plus")).Times(1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"(
      {
      "plusProfile": {
          "facet": "hulu.com",
          "plusEmail" : {
            "plusAddress": "plusthree@plus.plus"
          }
        }
    }
    )");
  // Same for the second callback.
  EXPECT_CALL(second_request, Run("plusfour@plus.plus")).Times(1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"(
      {
      "plusProfile": {
          "facet": "netflix.com",
          "plusEmail" : {
            "plusAddress": "plusfour@plus.plus"
          }
        }
    }
    )");
}

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, ReservePlusAddress_IssuesCorrectRequest) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  std::string site = "https://foobar.com";
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.ReservePlusAddress(site, base::DoNothing());

  // Validate that the V1 Create request uses the right url and requests method.
  EXPECT_EQ(last_request.url, fullReserveEndpoint);
  EXPECT_EQ(last_request.method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes "myToken".
  std::string authorization_value;
  last_request.headers.GetHeader("Authorization", &authorization_value);
  EXPECT_EQ(authorization_value, "Bearer " + token);

  // Validate the request payload.
  ASSERT_NE(last_request.request_body, nullptr);
  ASSERT_EQ(last_request.request_body->elements()->size(), 1u);
  absl::optional<base::Value> body =
      base::JSONReader::Read(last_request.request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  ASSERT_TRUE(body.has_value() && body->is_dict());
  std::string* facet_entry = body->GetDict().FindString("facet");
  ASSERT_NE(facet_entry, nullptr);
  EXPECT_EQ(*facet_entry, site);
}

TEST_F(PlusAddressClientRequests,
       ReservePlusAddress_EnqueuedUntilOAuthTokenFetched) {
  identity_test_env.MakePrimaryAccountAvailable("foo@plus.plus",
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  std::string site = "https://foobar.com";
  base::MockOnceCallback<void(const std::string&)> callback;
  // Make a request when the PlusAddressClient has an expired OAuth token.
  EXPECT_CALL(callback, Run).Times(0);
  client.ReservePlusAddress(site, callback.Get());

  // Verify that ReservePlusAddress hasn't already sent the network request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 0);

  // ReservePlusAddress will  run `callback` after an OAuth token is retrieved.
  EXPECT_CALL(callback, Run).Times(1);
  identity_test_env
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {test_scope});

  // Unblock the pending request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullReserveEndpoint,
                                                            R"(
    {
      "plusProfile": {
          "unwanted": 123,
          "facet": "youtube.com",
          "plusEmail" : {
            "plusAddress": "plusone@plus.plus"
          }
        },
      "unwanted": "abc"
    }
    )");
}

// For tests that cover successful but unexpected server responses, see the
// PlusAddressParsing.FromV1Create tests.
TEST_F(PlusAddressClientRequests, ReservePlusAddress_RunsCallbackOnSuccess) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());
  std::string site = "https://foobar.com";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.ReservePlusAddress(site, on_response_parsed.Get());
  // Fulfill the request and the callback should be run
  EXPECT_CALL(on_response_parsed, Run("plusone@plus.plus")).Times(1);
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  const std::string json = R"(
    {
      "plusProfile": {
          "unwanted": 123,
          "facet": "youtube.com",
          "plusEmail" : {
            "plusAddress": "plusone@plus.plus"
          }
        },
      "unwanted": "abc"
    }
    )";
  test_url_loader_factory.SimulateResponseForPendingRequest(fullReserveEndpoint,
                                                            json);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kReserve), latency, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kReserve), 200,
      1);
  histogram_tester.ExpectUniqueSample(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kReserve),
      json.size(), 1);
}

TEST_F(PlusAddressClientRequests,
       ReservePlusAddress_FailedRequestDoesntRunCallback) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());
  std::string site = "https://foobar.com";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.ReservePlusAddress(site, on_response_parsed.Get());

  // The request fails and the callback is never run
  EXPECT_CALL(on_response_parsed, Run).Times(0);
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      fullReserveEndpoint, "", net::HTTP_NOT_FOUND));
  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kReserve), latency, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kReserve),
      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectTotalCount(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kReserve), 0);
}

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, ConfirmPlusAddress_IssuesCorrectRequest) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  std::string site = "https://foobar.com";
  std::string plus_address = "plus@plus.plus";
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.ConfirmPlusAddress(site, plus_address, base::DoNothing());

  // Validate that the V1 Create request uses the right url and requests method.
  EXPECT_EQ(last_request.url, fullConfirmEndpoint);
  EXPECT_EQ(last_request.method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes "myToken".
  std::string authorization_value;
  last_request.headers.GetHeader("Authorization", &authorization_value);
  EXPECT_EQ(authorization_value, "Bearer " + token);

  // Validate the request payload.
  ASSERT_NE(last_request.request_body, nullptr);
  ASSERT_EQ(last_request.request_body->elements()->size(), 1u);
  absl::optional<base::Value> body =
      base::JSONReader::Read(last_request.request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  ASSERT_TRUE(body.has_value() && body->is_dict());
  std::string* facet_entry = body->GetDict().FindString("facet");
  ASSERT_NE(facet_entry, nullptr);
  EXPECT_EQ(*facet_entry, site);
}

TEST_F(PlusAddressClientRequests,
       ConfirmPlusAddress_EnqueuedUntilOAuthTokenFetched) {
  identity_test_env.MakePrimaryAccountAvailable("foo@plus.plus",
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  std::string site = "https://foobar.com";
  base::MockOnceCallback<void(const std::string&)> callback;
  // Make a request when the PlusAddressClient has an expired OAuth token.
  EXPECT_CALL(callback, Run).Times(0);
  client.ConfirmPlusAddress(site, "plus+plus@plus.plus", callback.Get());

  // Verify that ConfirmPlusAddress hasn't already sent the network request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 0);

  // ConfirmPlusAddress will run `callback` after an OAuth token is retrieved.
  EXPECT_CALL(callback, Run).Times(1);
  identity_test_env
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {test_scope});

  // Unblock the pending request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullConfirmEndpoint,
                                                            R"(
    {
      "plusProfile": {
          "unwanted": 123,
          "facet": "youtube.com",
          "plusEmail" : {
            "plusAddress": "plusone@plus.plus"
          }
        },
      "unwanted": "abc"
    }
    )");
}

TEST_F(PlusAddressClientRequests, ConfirmPlusAddress_RunsCallbackOnSuccess) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());
  std::string site = "https://foobar.com";
  std::string plus_address = "plus@plus.plus";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.ConfirmPlusAddress(site, plus_address, on_response_parsed.Get());
  // Fulfill the request and the callback should be run
  EXPECT_CALL(on_response_parsed, Run(plus_address)).Times(1);
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  const std::string json = R"(
    {
      "plusProfile": {
          "unwanted": 123,
          "facet": "youtube.com",
          "plusEmail" : {
            "plusAddress": "plus@plus.plus"
          }
        },
      "unwanted": "abc"
    }
    )";
  test_url_loader_factory.SimulateResponseForPendingRequest(fullConfirmEndpoint,
                                                            json);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kCreate), latency, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kCreate), 200, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kCreate),
      json.size(), 1);
}

TEST_F(PlusAddressClientRequests,
       ConfirmPlusAddress_FailedRequestDoesntRunCallback) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());
  std::string site = "https://foobar.com";
  std::string plus_address = "plus@plus.plus";

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.ConfirmPlusAddress(site, plus_address, on_response_parsed.Get());

  // The request fails and the callback is never run
  EXPECT_CALL(on_response_parsed, Run).Times(0);
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      fullConfirmEndpoint, "", net::HTTP_NOT_FOUND));
  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kCreate), latency, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kCreate),
      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectTotalCount(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kCreate), 0);
}

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, GetAllPlusAddressesV1_IssuesCorrectRequest) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.GetAllPlusAddresses(base::DoNothing());

  // Validate that the V1 List request uses the right url and requests method.
  EXPECT_EQ(last_request.url, fullProfileEndpoint);
  EXPECT_EQ(last_request.method, net::HttpRequestHeaders::kGetMethod);
  // Validate the Authorization header includes "myToken".
  std::string authorization_value;
  last_request.headers.GetHeader("Authorization", &authorization_value);
  EXPECT_EQ(authorization_value, "Bearer " + token);
}

TEST_F(PlusAddressClientRequests,
       GetAllPlusAddresses_EnqueuedUntilOAuthTokenFetched) {
  identity_test_env.MakePrimaryAccountAvailable("foo@plus.plus",
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  base::MockOnceCallback<void(const PlusAddressMap&)> callback;
  // Make a request when the PlusAddressClient has an expired OAuth token.
  EXPECT_CALL(callback, Run).Times(0);
  client.GetAllPlusAddresses(callback.Get());

  // Verify that GetAllPlusAddresses hasn't already sent the network request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 0);

  // GetAllPlusAddresses will run `callback`  after an OAuth token is retrieved.
  EXPECT_CALL(callback, Run).Times(1);
  identity_test_env
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {test_scope});

  // Unblock the pending request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"(
    {
      "plusProfiles": [
          {
            "unwanted": 123,
            "facet": "youtube.com",
            "plusEmail" : {
              "plusAddress": "plusone@plus.plus"
            }
          }
        ]
    }
    )");
}

// For tests that cover successful but unexpected server responses, see the
// PlusAddressParsing.FromV1List tests.
TEST_F(PlusAddressClientRequests, GetAllPlusAddressesV1_RunsCallbackOnSuccess) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());

  base::MockOnceCallback<void(const PlusAddressMap&)> on_response_parsed;
  // Initiate a request...
  client.GetAllPlusAddresses(on_response_parsed.Get());
  PlusAddressMap expected({{"google.com", "plusone@plus.plus"},
                           {"netflix.com", "plusplustwo@plus.plus"}});
  // Fulfill the request and the callback should be run
  EXPECT_CALL(on_response_parsed, Run(expected)).Times(1);
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  const std::string json = R"(
    {
      "plusProfiles": [
        {
          "unwanted": 123,
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "plusone@plus.plus"
          }
        },
        {
          "facet": "netflix.com",
          "plusEmail" : {
            "plusAddress": "plusplustwo@plus.plus"
          }
        }
      ],
      "unwanted": "abc"
    }
    )";
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            json);
  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kList), latency, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kList), 200, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kList),
      json.size(), 1);
}

TEST_F(PlusAddressClientRequests,
       GetAllPlusAddressesV1_FailedRequestDoesntRunCallback) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);
  client.SetClockForTesting(test_clock());

  base::MockOnceCallback<void(const PlusAddressMap&)> on_response_parsed;
  // Initiate a request...
  client.GetAllPlusAddresses(on_response_parsed.Get());

  // The request fails and the callback is never run
  EXPECT_CALL(on_response_parsed, Run).Times(0);
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      fullProfileEndpoint, "", net::HTTP_NOT_FOUND));
  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kList), latency, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kList),
      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectTotalCount(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kList), 0);
}

TEST_F(PlusAddressClientRequests,
       GetAllPlusAddressesV1_WhenLoadingRequest_NewRequestsAreDropped) {
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_token_info);

  base::MockOnceCallback<void(const PlusAddressMap&)> first_request;
  // Send two requests in quick succession
  client.GetAllPlusAddresses(first_request.Get());
  EXPECT_DCHECK_DEATH(client.GetAllPlusAddresses(base::DoNothing()));

  // The first callback should be run once the server responds.
  PlusAddressMap expected;
  EXPECT_CALL(first_request, Run(expected)).Times(1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"(
    {
      "plusProfiles": []
    }
    )");
}

TEST(PlusAddressClient, ChecksUrlParamIsValidGurl) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  std::string server_url = "https://foo.com/";
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      kFeature, {{kEnterprisePlusAddressServerUrl.name, server_url}});
  PlusAddressClient client(
      identity_test_env.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  ASSERT_TRUE(client.GetServerUrlForTesting().has_value());
  EXPECT_EQ(client.GetServerUrlForTesting().value(), server_url);
}

TEST(PlusAddressClient, RejectsNonUrlStrings) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      kFeature, {{kEnterprisePlusAddressServerUrl.name, "kirubeldotcom"}});
  PlusAddressClient client(
      identity_test_env.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_FALSE(client.GetServerUrlForTesting().has_value());
}

class PlusAddressAuthToken : public ::testing::Test {
 public:
  PlusAddressAuthToken() {
    // Init the feature param to add `test_scope_` to GetUnconsentedOAuth2Scopes
    features_.InitAndEnableFeatureWithParameters(
        kFeature, {{kEnterprisePlusAddressOAuthScope.name, test_scope_}});

    // Time-travel back to 1970 so that we can test with base::Time::FromDoubleT
    clock_.SetNow(base::Time::FromDoubleT(1));
  }

 protected:
  // A blocking helper that signs the user in and gets an OAuth token with our
  // test scope.
  // Note: this blocks indefinitely if there are no listeners for token
  // creation. This means it must be called after GetAuthToken.
  void WaitForSignInAndToken() {
    identity_test_env_.MakePrimaryAccountAvailable(
        test_email_address_, signin::ConsentLevel::kSignin);
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            test_token_, test_token_expiration_time_, "id", test_scopes_);
  }

  // A blocking helper that gets an OAuth token for our test scope that expires
  // at `expiration_time`.
  void WaitForToken(base::Time expiration_time) {
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            test_token_, expiration_time, "id", test_scopes_);
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  base::Clock* test_clock() { return &clock_; }

  std::string test_token_ = "access_token";
  std::string test_scope_ = "https://googleapis.com/test.scope";
  signin::ScopeSet test_scopes_ = {test_scope_};
  base::Time test_token_expiration_time_ = base::Time::FromDoubleT(1000);

  base::HistogramTester histogram_tester;

 private:
  // Required by `signin::IdentityTestEnvironment`.
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  base::test::ScopedFeatureList features_;
  base::SimpleTestClock clock_;
  std::string test_email_address_ = "foo@gmail.com";
};

TEST_F(PlusAddressAuthToken, RequestedBeforeSignin) {
  PlusAddressClient client(
      identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());

  bool ran_callback = false;
  client.GetAuthToken(
      base::BindLambdaForTesting([&]() { ran_callback = true; }));

  // The callback is run only after signin.
  EXPECT_FALSE(ran_callback);
  WaitForSignInAndToken();
  EXPECT_TRUE(ran_callback);
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 1)));
}

TEST_F(PlusAddressAuthToken, RequestedUserNeverSignsIn) {
  PlusAddressClient client(
      identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run).Times(0);
  client.GetAuthToken(callback.Get());
  histogram_tester.ExpectTotalCount(kPlusAddressOauthErrorHistogram, 0);
}

TEST_F(PlusAddressAuthToken, RequestedAfterExpiration) {
  PlusAddressClient client(
      identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  // Make an initial OAuth token request.
  base::MockOnceClosure first_callback;
  client.GetAuthToken(first_callback.Get());
  EXPECT_CALL(first_callback, Run).Times(1);
  histogram_tester.ExpectTotalCount(kPlusAddressOauthErrorHistogram, 0);

  // Sign in, get a token, and fast-forward to after it is expired.
  WaitForSignInAndToken();
  base::Time now = test_token_expiration_time_ + base::Seconds(1);
  AdvanceTimeTo(now);

  // Issue another request for an OAuth token.
  base::MockOnceClosure second_callback;
  client.GetAuthToken(second_callback.Get());

  // Callback is only run once the new OAuth token request has completed.
  EXPECT_CALL(second_callback, Run).Times(1);
  WaitForToken(/*expiration_time=*/now + base::Hours(1));
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 2)));
}

}  // namespace plus_addresses
