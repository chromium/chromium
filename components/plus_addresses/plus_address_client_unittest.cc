// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/google_service_auth_error.h"
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

  std::string MakeCreationResponse(const std::string& facet,
                                   const std::string& plus_address,
                                   bool is_confirmed) {
    return test::MakeCreationResponse(
        PlusProfile{.facet = facet,
                    .plus_address = plus_address,
                    .is_confirmed = is_confirmed});
  }

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
  std::string email_address = "foo@plus.plus";

  // Issue all requests starting at this time to test the latency metrics.
  base::Time start_time = base::Time::FromSecondsSinceUnixEpoch(1);

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
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  client.CreatePlusAddress(origin, base::DoNothing());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  // Validate that the V1 Create request uses the right url and request method.
  EXPECT_EQ(last_request.url, fullProfileEndpoint);
  EXPECT_EQ(last_request.method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes `token`.
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
  EXPECT_EQ(*facet_entry, origin.Serialize());
}

TEST_F(PlusAddressClientRequests, CreatePlusAddressV1_RequestsOauthToken) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  // Mock out the response from the remote service as a string. The service
  // refers to this as a facet, so matching its nomenclature.
  std::string facet = origin.Serialize();
  base::MockOnceCallback<void(const std::string&)> callback;
  EXPECT_CALL(callback, Run).Times(0);
  client.CreatePlusAddress(origin, callback.Get());
  ASSERT_TRUE(identity_test_env.IsAccessTokenRequestPending());

  // Verify that CreatePlusAddress hasn't already sent the network request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 0);

  // CreatePlusAddress will run `callback` after an OAuth token is retrieved.
  EXPECT_CALL(callback, Run).Times(1);
  identity_test_env
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {test_scope});

  // Unblock the pending request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      fullProfileEndpoint,
      MakeCreationResponse(facet, "unused+plus@plus.plus", true));
}

// For tests that cover successful but unexpected server responses, see the
// PlusAddressParsing.FromV1Create tests.
TEST_F(PlusAddressClientRequests, CreatePlusAddressV1_RunsCallbackOnSuccess) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetClockForTesting(test_clock());
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  std::string plus_address = "plusone@plus.plus";

  base::MockOnceCallback<void(const std::string&)> on_complete;
  // Initiate a request...
  client.CreatePlusAddress(origin, on_complete.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());
  // Fulfill the request and the callback should be run
  EXPECT_CALL(on_complete, Run(plus_address)).Times(1);

  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  const std::string json = MakeCreationResponse(facet, plus_address, true);
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
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetClockForTesting(test_clock());
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();

  base::MockOnceCallback<void(const std::string&)> on_response_parsed;
  // Initiate a request...
  client.CreatePlusAddress(origin, on_response_parsed.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

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
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);

  base::MockOnceCallback<void(const std::string&)> first_request;
  base::MockOnceCallback<void(const std::string&)> second_request;

  const url::Origin origin_1 = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet_1 = origin_1.Serialize();
  const url::Origin origin_2 = url::Origin::Create(GURL("https://barfoo.com"));
  std::string facet_2 = origin_2.Serialize();
  std::string plus_address_1 = "plus1@plus.plus";
  std::string plus_address_2 = "plus2@plus.plus";

  // Send two requests in quick succession
  client.CreatePlusAddress(origin_1, first_request.Get());
  client.CreatePlusAddress(origin_2, second_request.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  // The first callback should be run once the server responds to its request.
  PlusAddressMap expected;
  EXPECT_CALL(first_request, Run(plus_address_1)).Times(1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      fullProfileEndpoint, MakeCreationResponse(facet_1, plus_address_1, true));
  // Same for the second callback.
  EXPECT_CALL(second_request, Run(plus_address_2)).Times(1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      fullProfileEndpoint, MakeCreationResponse(facet_2, plus_address_2, true));
}

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, ReservePlusAddress_IssuesCorrectRequest) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_test_env.identity_manager(),
                           scoped_shared_url_loader_factory);
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  client.ReservePlusAddress(origin, base::DoNothing());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

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
  EXPECT_EQ(*facet_entry, facet);
}

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, ConfirmPlusAddress_IssuesCorrectRequest) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  std::string plus_address = "plus@plus.plus";
  client.ConfirmPlusAddress(origin, plus_address, base::DoNothing());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

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
  EXPECT_EQ(*facet_entry, facet);
}

// Tests the behavior of the PlusAddressCreationRequests (Reserve+Confirm) which
// have identical expectations outside of the method signature.
class PlusAddressCreationRequests
    : public PlusAddressClientRequests,
      public testing::WithParamInterface<PlusAddressNetworkRequestType> {
 public:
  PlusAddressCreationRequests()
      : client_(identity_manager, scoped_shared_url_loader_factory) {
    identity_test_env.MakePrimaryAccountAvailable(
        email_address, signin::ConsentLevel::kSignin);
    client_.SetClockForTesting(test_clock());
  }

 protected:
  std::string Endpoint() {
    if (GetParam() == PlusAddressNetworkRequestType::kReserve) {
      return fullReserveEndpoint;
    }
    if (GetParam() == PlusAddressNetworkRequestType::kCreate) {
      return fullConfirmEndpoint;
    }
    NOTREACHED_NORETURN();
  }
  void MakeCreationRequest(PlusAddressRequestCallback callback) {
    if (GetParam() == PlusAddressNetworkRequestType::kReserve) {
      client_.ReservePlusAddress(origin_, std::move(callback));
    } else if (GetParam() == PlusAddressNetworkRequestType::kCreate) {
      client_.ConfirmPlusAddress(origin_, plus_address_, std::move(callback));
    } else {
      NOTREACHED();
    }
  }
  std::string LatencyHistogram() { return LatencyHistogramFor(GetParam()); }
  std::string ResponseCodeHistogram() {
    return ResponseCodeHistogramFor(GetParam());
  }
  std::string ResponseByteSizeHistogram() {
    return ResponseByteSizeHistogramFor(GetParam());
  }

  const url::Origin origin_ = url::Origin::Create(GURL("https://foobar.com"));
  const std::string plus_address_ = "plus@plus.plus";
  base::TimeDelta latency_ = base::Milliseconds(2400);

 private:
  PlusAddressClient client_;
};

// Verifies ability to support making multiple requests at once.
// Note: Create() is not idempotent, but that is ignored for this test.
TEST_P(PlusAddressCreationRequests, HandlesConcurrentRequests) {
  base::test::TestFuture<const PlusProfileOrError&> first_request;
  base::test::TestFuture<const PlusProfileOrError&> second_request;

  // Send two requests in quick succession.
  MakeCreationRequest(first_request.GetCallback());
  MakeCreationRequest(second_request.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  // The first callback should be run once the server responds to its request.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      Endpoint(),
      MakeCreationResponse(origin_.Serialize(), plus_address_, true));
  EXPECT_TRUE(first_request.IsReady());

  // Same for the second callback.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      Endpoint(),
      MakeCreationResponse(origin_.Serialize(), plus_address_, true));
  EXPECT_TRUE(second_request.IsReady());
}

TEST_P(PlusAddressCreationRequests, RequestsOauthToken) {
  // Make a request when the PlusAddressClient has an expired OAuth token.
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(future.GetCallback());
  ASSERT_FALSE(future.IsReady());
  ASSERT_TRUE(identity_test_env.IsAccessTokenRequestPending());

  // Verify that ConfirmPlusAddress hasn't already sent the network request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 0);

  // ConfirmPlusAddress will run `callback` after an OAuth token is retrieved.
  identity_test_env
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {test_scope});

  // Unblock the pending request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      Endpoint(),
      MakeCreationResponse(origin_.Serialize(), plus_address_, true));
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get()->plus_address, plus_address_);
}

TEST_P(PlusAddressCreationRequests, RunCallbackOnSuccess) {
  // Initiate a request...
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(future.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  // Fulfill the request and the appropriate callback should be run.
  AdvanceTimeTo(start_time + latency_);
  const std::string json =
      MakeCreationResponse(origin_.Serialize(), plus_address_, true);
  test_url_loader_factory.SimulateResponseForPendingRequest(Endpoint(), json);

  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->plus_address, plus_address_);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(LatencyHistogram(), latency_, 1);
  histogram_tester.ExpectUniqueSample(ResponseCodeHistogram(), 200, 1);
  histogram_tester.ExpectUniqueSample(ResponseByteSizeHistogram(), json.size(),
                                      1);
}

TEST_P(PlusAddressCreationRequests, RunCallbackOnNetworkError) {
  // Initiate a request...
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(future.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  AdvanceTimeTo(start_time + latency_);
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      Endpoint(), "", net::HTTP_NOT_FOUND));

  // The request fails and the appropriate callback is run.
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().type(),
            PlusAddressRequestErrorType::kNetworkError);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(LatencyHistogram(), latency_, 1);
  histogram_tester.ExpectUniqueSample(ResponseCodeHistogram(),
                                      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectTotalCount(ResponseByteSizeHistogram(), 0);
}

TEST_P(PlusAddressCreationRequests, RunCallbackOnClientError) {
  // Initiate a request...
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(future.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  // Return a response missing all of the expected fields.
  AdvanceTimeTo(start_time + latency_);
  const std::string json = "{}";
  test_url_loader_factory.SimulateResponseForPendingRequest(Endpoint(), json);

  // The request fails and the appropriate callback is run.
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().type(),
            PlusAddressRequestErrorType::kParsingError);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(LatencyHistogram(), latency_, 1);
  histogram_tester.ExpectUniqueSample(ResponseCodeHistogram(), net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(ResponseByteSizeHistogram(), json.size(),
                                      1);
}

TEST_P(PlusAddressCreationRequests, RunCallbackOnOauthError) {
  // Initiate a request...
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(future.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Verify that no network requests are made.
  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);

  // The callback is still run with an OAuth error.
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().type(),
            PlusAddressRequestErrorType::kOAuthError);

  // Verify expected metrics.
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(
                  GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS, 1)));
  histogram_tester.ExpectTotalCount(LatencyHistogram(), 0);
  histogram_tester.ExpectTotalCount(ResponseCodeHistogram(), 0);
  histogram_tester.ExpectTotalCount(ResponseByteSizeHistogram(), 0);
}
INSTANTIATE_TEST_SUITE_P(
    All,
    PlusAddressCreationRequests,
    ::testing::Values(PlusAddressNetworkRequestType::kReserve,
                      PlusAddressNetworkRequestType::kCreate),
    [](const testing::TestParamInfo<PlusAddressCreationRequests::ParamType>&
           info) {
      return PlusAddressMetrics::PlusAddressNetworkRequestTypeToString(
          info.param);
    });

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressClientRequests, GetAllPlusAddressesV1_IssuesCorrectRequest) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.GetAllPlusAddresses(base::DoNothing());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  // Validate that the V1 List request uses the right url and requests method.
  EXPECT_EQ(last_request.url, fullProfileEndpoint);
  EXPECT_EQ(last_request.method, net::HttpRequestHeaders::kGetMethod);
  // Validate the Authorization header includes "myToken".
  std::string authorization_value;
  last_request.headers.GetHeader("Authorization", &authorization_value);
  EXPECT_EQ(authorization_value, "Bearer " + token);
}

TEST_F(PlusAddressClientRequests, GetAllPlusAddresses_RequestsOauthToken) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  base::test::TestFuture<const PlusAddressMap&> future;
  client.GetAllPlusAddresses(future.GetCallback());
  ASSERT_FALSE(future.IsReady());
  ASSERT_TRUE(identity_test_env.IsAccessTokenRequestPending());

  // Verify that GetAllPlusAddresses hasn't already sent the network request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 0);

  // GetAllPlusAddresses will run `callback`  after an OAuth token is retrieved.
  identity_test_env
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {test_scope});

  // Unblock the pending request.
  EXPECT_FALSE(future.IsReady());
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"({
      "plusProfiles": []
    })");
  EXPECT_TRUE(future.IsReady());
}

// For tests that cover successful but unexpected server responses, see the
// PlusAddressParsing.FromV1List tests.
TEST_F(PlusAddressClientRequests, GetAllPlusAddressesV1_RunsCallbackOnSuccess) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetClockForTesting(test_clock());

  base::test::TestFuture<const PlusAddressMap&> future;
  // Initiate a request...
  client.GetAllPlusAddresses(future.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  std::string plus_address_1 = "plus1@plus.plus";
  std::string plus_address_2 = "plus2@plus.plus";
  std::string facet_1 = "asdf.example";
  std::string facet_2 = "fdsa.example";

  PlusAddressMap expected(
      {{facet_1, plus_address_1}, {facet_2, plus_address_2}});
  // Fulfill the request and the callback should be run
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  const std::string json =
      test::MakeListResponse({PlusProfile{.facet = facet_1,
                                          .plus_address = plus_address_1,
                                          .is_confirmed = true},
                              PlusProfile{.facet = facet_2,
                                          .plus_address = plus_address_2,
                                          .is_confirmed = true}});
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            json);
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), expected);

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
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);
  client.SetClockForTesting(test_clock());

  base::test::TestFuture<const PlusAddressMap&> future;
  // Initiate a request...
  client.GetAllPlusAddresses(future.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());

  // The request fails and the callback is never run
  base::TimeDelta latency = base::Milliseconds(2400);
  AdvanceTimeTo(start_time + latency);
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      fullProfileEndpoint, "", net::HTTP_NOT_FOUND));
  EXPECT_FALSE(future.IsReady());

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kList), latency, 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kList),
      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectTotalCount(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kList), 0);
}

// TODO: crbug.com/1489268 - Reenable this test after fixing flakiness.
TEST_F(
    PlusAddressClientRequests,
    DISABLED_GetAllPlusAddressesV1_WhenLoadingRequest_NewRequestsAreDropped) {
  identity_test_env.MakePrimaryAccountAvailable(email_address,
                                                signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager, scoped_shared_url_loader_factory);

  base::test::TestFuture<const PlusAddressMap&> first;
  // Send two requests in quick succession
  client.GetAllPlusAddresses(first.GetCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      token, base::Time::Max());
  EXPECT_DCHECK_DEATH(client.GetAllPlusAddresses(base::DoNothing()));

  // The first callback should be run once the server responds.
  PlusAddressMap expected;
  test_url_loader_factory.SimulateResponseForPendingRequest(fullProfileEndpoint,
                                                            R"(
    {
      "plusProfiles": []
    }
    )");
  EXPECT_TRUE(first.IsReady());
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

    // Time-travel back to 1970 so that we can test with
    // base::Time::FromSecondsSinceUnixEpoch
    clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(1));
  }

 protected:
  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  base::Clock* test_clock() { return &clock_; }

  // Required by `signin::IdentityTestEnvironment`.
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  std::string test_email_address_ = "foo@gmail.com";
  std::string test_token_ = "access_token";
  std::string test_scope_ = "https://googleapis.com/test.scope";
  signin::ScopeSet test_scopes_ = {test_scope_};
  base::Time test_token_expiration_time_ =
      base::Time::FromSecondsSinceUnixEpoch(1000);

  base::HistogramTester histogram_tester;

 private:
  base::test::ScopedFeatureList features_;
  base::SimpleTestClock clock_;
};

TEST_F(PlusAddressAuthToken, RequestedBeforeSignin) {
  PlusAddressClient client(identity_manager(),
                           /* url_loader_factory= */ nullptr);

  base::test::TestFuture<absl::optional<std::string>> callback;
  client.GetAuthToken(callback.GetCallback());

  // The callback is run only after signin.
  EXPECT_FALSE(callback.IsReady());
  identity_test_env_.MakePrimaryAccountAvailable(test_email_address_,
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          test_token_, test_token_expiration_time_, "unused", test_scopes_);

  EXPECT_TRUE(callback.IsReady());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 1)));
}

TEST_F(PlusAddressAuthToken, RequestedUserNeverSignsIn) {
  PlusAddressClient client(identity_manager(),
                           /* url_loader_factory= */ nullptr);

  base::test::TestFuture<absl::optional<std::string>> callback;
  client.GetAuthToken(callback.GetCallback());
  EXPECT_FALSE(callback.IsReady());
  histogram_tester.ExpectTotalCount(kPlusAddressOauthErrorHistogram, 0);
}

TEST_F(PlusAddressAuthToken, RequestedAfterExpiration) {
  PlusAddressClient client(identity_manager(),
                           /* url_loader_factory= */ nullptr);
  // Make an initial OAuth token request.
  base::test::TestFuture<absl::optional<std::string>> first_callback;
  client.GetAuthToken(first_callback.GetCallback());

  // Sign in, get a token, and fast-forward to after it is expired.
  identity_test_env_.MakePrimaryAccountAvailable(test_email_address_,
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          test_token_, test_token_expiration_time_, "unused", test_scopes_);
  EXPECT_TRUE(first_callback.IsReady());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 1)));
  base::Time now = test_token_expiration_time_ + base::Seconds(1);
  AdvanceTimeTo(now);

  // Issue another request for an OAuth token.
  base::test::TestFuture<absl::optional<std::string>> second_callback;
  client.GetAuthToken(second_callback.GetCallback());

  // Callback is only run once the new OAuth token request has completed.
  EXPECT_FALSE(second_callback.IsReady());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          test_token_, now + base::Hours(1), "unused", test_scopes_);
  EXPECT_TRUE(second_callback.IsReady());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 2)));
}

TEST_F(PlusAddressAuthToken, AuthErrorWithMultipleAccounts) {
  // GetAuthToken() is only concerned with the primary token auth state.
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      test_email_address_, signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@foo.com");
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  PlusAddressClient client(identity_manager(),
                           /* url_loader_factory= */ nullptr);

  base::test::TestFuture<absl::optional<std::string>> callback;
  client.GetAuthToken(callback.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary.account_id, test_token_, base::Time::Max());
  EXPECT_EQ(callback.Get(), test_token_);
}

TEST_F(PlusAddressAuthToken, RequestWorks_ManyCallers) {
  identity_test_env_.MakePrimaryAccountAvailable(test_email_address_,
                                                 signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager(),
                           /* url_loader_factory= */ nullptr);

  // Issue several requests for an OAuth token.
  base::test::TestFuture<absl::optional<std::string>> first;
  base::test::TestFuture<absl::optional<std::string>> second;
  base::test::TestFuture<absl::optional<std::string>> third;
  client.GetAuthToken(first.GetCallback());
  client.GetAuthToken(second.GetCallback());
  client.GetAuthToken(third.GetCallback());

  // Although we failed to get a token, each callback should still be run.
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          test_token_, base::Time::Max(), "unused", test_scopes_);
  EXPECT_EQ(first.Get().value(), test_token_);
  EXPECT_EQ(second.Get().value(), test_token_);
  EXPECT_EQ(third.Get().value(), test_token_);
}

TEST_F(PlusAddressAuthToken, RequestFails_ManyCallers) {
  identity_test_env_.MakePrimaryAccountAvailable(test_email_address_,
                                                 signin::ConsentLevel::kSignin);
  PlusAddressClient client(identity_manager(),
                           /* url_loader_factory= */ nullptr);

  // Issue several requests for an OAuth token.
  base::test::TestFuture<absl::optional<std::string>> first;
  base::test::TestFuture<absl::optional<std::string>> second;
  base::test::TestFuture<absl::optional<std::string>> third;
  client.GetAuthToken(first.GetCallback());
  client.GetAuthToken(second.GetCallback());
  client.GetAuthToken(third.GetCallback());

  // Although we failed to get a token, each callback should still be run.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(first.Get().has_value());
  EXPECT_FALSE(second.Get().has_value());
  EXPECT_FALSE(third.Get().has_value());
}

}  // namespace plus_addresses
