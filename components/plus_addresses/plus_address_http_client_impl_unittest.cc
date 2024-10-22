// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_http_client_impl.h"

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_http_client_impl_test_api.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

using ::testing::SizeIs;

constexpr char kServerBaseUrl[] = "https://enterprise.foo/";
constexpr char kTestScope[] = "scope";
constexpr char kEmailAddress[] = "foo@plus.plus";

constexpr base::TimeDelta kLatency = base::Milliseconds(2400);

constexpr char kPlusAddressOauthErrorHistogram[] =
    "PlusAddresses.NetworkRequest.OauthError";

std::string LatencyHistogramFor(PlusAddressNetworkRequestType type) {
  return base::ReplaceStringPlaceholders(
      "PlusAddresses.NetworkRequest.$1.Latency",
      {metrics::PlusAddressNetworkRequestTypeToString(type)},
      /*offsets=*/nullptr);
}

std::string ResponseCodeHistogramFor(PlusAddressNetworkRequestType type) {
  return base::ReplaceStringPlaceholders(
      "PlusAddresses.NetworkRequest.$1.ResponseCode",
      {metrics::PlusAddressNetworkRequestTypeToString(type)},
      /*offsets=*/nullptr);
}

std::string ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType type) {
  return base::ReplaceStringPlaceholders(
      "PlusAddresses.NetworkRequest.$1.ResponseByteSize",
      {metrics::PlusAddressNetworkRequestTypeToString(type)},
      /*offsets=*/nullptr);
}

// Tests that use fake out the URL loading and issues requests to the enterprise
// provided server.
class PlusAddressHttpClientRequests : public ::testing::Test {
 public:
  PlusAddressHttpClientRequests()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    features_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled,
        {{features::kEnterprisePlusAddressServerUrl.name, kServerBaseUrl},
         {features::kEnterprisePlusAddressOAuthScope.name, kTestScope}});
    identity_test_env_.MakePrimaryAccountAvailable(
        kEmailAddress, signin::ConsentLevel::kSignin);
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request_ = request;
        }));
    InitClient();
  }

 protected:
  // Runtime constants:
  const std::string kFullProfileEndpoint =
      base::StrCat({kServerBaseUrl, kServerPlusProfileEndpoint});
  const std::string kFullReserveEndpoint =
      base::StrCat({kServerBaseUrl, kServerReservePlusAddressEndpoint});
  const std::string kFullCreateEndpoint =
      base::StrCat({kServerBaseUrl, kServerCreatePlusAddressEndpoint});
  const std::string kFullPreallocateEndpoint =
      base::StrCat({kServerBaseUrl, kServerPreallocatePlusAddressEndpoint});
  // This is a `std::string` to allow easier concatenation via `operator+`.
  const std::string kToken = "myToken";

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void InitClient() {
    client_.emplace(identity_manager(), shared_loader_factory());
  }

  PlusAddressHttpClientImpl& client() { return *client_; }
  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  const network::ResourceRequest& last_request() const { return last_request_; }
  const scoped_refptr<network::SharedURLLoaderFactory>&
  shared_loader_factory() {
    return test_shared_loader_factory_;
  }
  network::TestURLLoaderFactory& url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder decoder_;

  // The last network request made.
  network::ResourceRequest last_request_;

  // `client_` is wrapped in an optional to defer creation until after features
  // are initialized. After fixture creation, this is never empty.
  std::optional<PlusAddressHttpClientImpl> client_;
};

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressHttpClientRequests, ReservePlusAddress_IssuesCorrectRequest) {
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  client().ReservePlusAddress(origin, /*refresh=*/false, base::DoNothing());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  // Validate that the V1 Create request uses the right url and requests method.
  EXPECT_EQ(last_request().url, kFullReserveEndpoint);
  EXPECT_EQ(last_request().method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes "myToken".
  EXPECT_EQ(
      last_request().headers.GetHeader("Authorization").value_or(std::string()),
      "Bearer " + kToken);

  // Validate the request payload.
  ASSERT_NE(last_request().request_body, nullptr);
  ASSERT_EQ(last_request().request_body->elements()->size(), 1u);
  std::optional<base::Value> body =
      base::JSONReader::Read(last_request()
                                 .request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  ASSERT_TRUE(body.has_value() && body->is_dict());
  std::string* facet_entry = body->GetDict().FindString("facet");
  ASSERT_NE(facet_entry, nullptr);
  EXPECT_EQ(*facet_entry, facet);
}

// Tests that the reserve request contains the expected data when the refresh
// flag is set.
TEST_F(PlusAddressHttpClientRequests,
       ReservePlusAddress_IssuesCorrectRequestWithRefreshFlag) {
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  client().ReservePlusAddress(origin, /*refresh=*/true, base::DoNothing());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  // Validate that the V1 Create request uses the right url and requests method.
  EXPECT_EQ(last_request().url, kFullReserveEndpoint);
  EXPECT_EQ(last_request().method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes "myToken".
  EXPECT_EQ(
      last_request().headers.GetHeader("Authorization").value_or(std::string()),
      "Bearer " + kToken);

  // Validate the request payload.
  ASSERT_NE(last_request().request_body, nullptr);
  ASSERT_THAT(*last_request().request_body->elements(), SizeIs(1));
  std::optional<base::Value> body =
      base::JSONReader::Read(last_request()
                                 .request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  ASSERT_TRUE(body.has_value() && body->is_dict());
  std::string* facet_entry = body->GetDict().FindString("facet");
  ASSERT_NE(facet_entry, nullptr);
  EXPECT_EQ(*facet_entry, facet);
  EXPECT_TRUE(
      body->GetDict().FindBool("refresh_email_address").value_or(false));
}

// Ensures the request sent by Chrome matches what we intended.
TEST_F(PlusAddressHttpClientRequests, ConfirmPlusAddress_IssuesCorrectRequest) {
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  client().ConfirmPlusAddress(origin, PlusAddress("plus@plus.plus"),
                              base::DoNothing());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  // Validate that the V1 Create request uses the right url and requests method.
  EXPECT_EQ(last_request().url, kFullCreateEndpoint);
  EXPECT_EQ(last_request().method, net::HttpRequestHeaders::kPutMethod);
  // Validate the Authorization header includes "myToken".
  EXPECT_EQ(
      last_request().headers.GetHeader("Authorization").value_or(std::string()),
      "Bearer " + kToken);

  // Validate the request payload.
  ASSERT_NE(last_request().request_body, nullptr);
  ASSERT_EQ(last_request().request_body->elements()->size(), 1u);
  std::optional<base::Value> body =
      base::JSONReader::Read(last_request()
                                 .request_body->elements()
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
    : public PlusAddressHttpClientRequests,
      public testing::WithParamInterface<PlusAddressNetworkRequestType> {
 public:
  PlusAddressCreationRequests() = default;

 protected:
  std::string Endpoint() {
    if (GetParam() == PlusAddressNetworkRequestType::kReserve) {
      return kFullReserveEndpoint;
    }
    if (GetParam() == PlusAddressNetworkRequestType::kCreate) {
      return kFullCreateEndpoint;
    }
    NOTREACHED();
  }
  void MakeCreationRequest(const PlusProfile& profile,
                           PlusAddressRequestCallback callback) {
    url::Origin origin =
        url::Origin::Create(GURL(profile.facet.canonical_spec()));
    if (GetParam() == PlusAddressNetworkRequestType::kReserve) {
      client().ReservePlusAddress(origin, /*refresh=*/false,
                                  std::move(callback));
    } else if (GetParam() == PlusAddressNetworkRequestType::kCreate) {
      client().ConfirmPlusAddress(origin, profile.plus_address,
                                  std::move(callback));
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
  std::string LatencyHistogram() { return LatencyHistogramFor(GetParam()); }
  std::string ResponseCodeHistogram() {
    return ResponseCodeHistogramFor(GetParam());
  }
  std::string ResponseByteSizeHistogram() {
    return ResponseByteSizeHistogramFor(GetParam());
  }
};

// Verifies ability to support making multiple requests at once.
// Note: Create() is not idempotent, but that is ignored for this test.
TEST_P(PlusAddressCreationRequests, HandlesConcurrentRequests) {
  const PlusProfile profile = test::CreatePlusProfile();
  base::test::TestFuture<const PlusProfileOrError&> first_request;
  base::test::TestFuture<const PlusProfileOrError&> second_request;

  // Send two requests in quick succession.
  MakeCreationRequest(profile, first_request.GetCallback());
  MakeCreationRequest(profile, second_request.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  // The first callback should be run once the server responds to its request.
  url_loader_factory().SimulateResponseForPendingRequest(
      Endpoint(), test::MakeCreationResponse(profile));
  EXPECT_TRUE(first_request.Wait());

  // Same for the second callback.
  url_loader_factory().SimulateResponseForPendingRequest(
      Endpoint(), test::MakeCreationResponse(profile));
  EXPECT_TRUE(second_request.Wait());
}

TEST_P(PlusAddressCreationRequests, RequestsOauthToken) {
  const PlusProfile profile = test::CreatePlusProfile();
  // Make a request when the PlusAddressHttpClient has an expired OAuth token.
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(profile, future.GetCallback());
  ASSERT_FALSE(future.IsReady());
  ASSERT_TRUE(identity_env().IsAccessTokenRequestPending());

  // Verify that ConfirmPlusAddress hasn't already sent the network request.
  ASSERT_EQ(url_loader_factory().NumPending(), 0);

  // ConfirmPlusAddress will run `callback` after an OAuth token is retrieved.
  identity_env()
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          "token", base::Time::Max(), "id", {kTestScope});

  // Unblock the pending request.
  ASSERT_EQ(url_loader_factory().NumPending(), 1);
  ASSERT_FALSE(future.IsReady());
  url_loader_factory().SimulateResponseForPendingRequest(
      Endpoint(), test::MakeCreationResponse(profile));
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get()->plus_address, profile.plus_address);
}

TEST_P(PlusAddressCreationRequests, RunCallbackOnSuccess) {
  const PlusProfile profile = test::CreatePlusProfile();
  base::HistogramTester histogram_tester;
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(profile, future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  // Fulfill the request and the appropriate callback should be run.
  FastForwardBy(kLatency);
  const std::string json = test::MakeCreationResponse(profile);
  url_loader_factory().SimulateResponseForPendingRequest(Endpoint(), json);

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->plus_address, profile.plus_address);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(LatencyHistogram(), kLatency, 1);
  histogram_tester.ExpectUniqueSample(ResponseCodeHistogram(), 200, 1);
  histogram_tester.ExpectUniqueSample(ResponseByteSizeHistogram(), json.size(),
                                      1);
}

// Tests that calls to the `Reserve` and `Create` endpoints run the callback
// with an error if the network request experienced an error. Also checks that
// the error includes the HTTP response code.
TEST_P(PlusAddressCreationRequests, RunCallbackOnNetworkError) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(test::CreatePlusProfile(), future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  FastForwardBy(kLatency);
  //  TODO (kaklilu): Checks behavior when response body isn't null.
  EXPECT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      Endpoint(), "", net::HTTP_NOT_FOUND));

  // The request fails and the appropriate callback is run.
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            PlusAddressRequestError::AsNetworkError(net::HTTP_NOT_FOUND));

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(LatencyHistogram(), kLatency, 1);
  histogram_tester.ExpectUniqueSample(ResponseCodeHistogram(),
                                      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectTotalCount(ResponseByteSizeHistogram(), 0);
}

TEST_P(PlusAddressCreationRequests, RunCallbackOnClientError) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(test::CreatePlusProfile(), future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  // Return a response missing all of the expected fields.
  FastForwardBy(kLatency);
  const std::string json = "{}";
  url_loader_factory().SimulateResponseForPendingRequest(Endpoint(), json);

  // The request fails and the appropriate callback is run.
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().type(),
            PlusAddressRequestErrorType::kParsingError);

  // Verify expected metrics.
  histogram_tester.ExpectUniqueTimeSample(LatencyHistogram(), kLatency, 1);
  histogram_tester.ExpectUniqueSample(ResponseCodeHistogram(), net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(ResponseByteSizeHistogram(), json.size(),
                                      1);
}

TEST_P(PlusAddressCreationRequests, RunCallbackOnOauthError) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<const PlusProfileOrError&> future;
  MakeCreationRequest(test::CreatePlusProfile(), future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Verify that no network requests are made.
  EXPECT_EQ(url_loader_factory().NumPending(), 0);

  // The callback is still run with an OAuth error.
  ASSERT_TRUE(future.Wait());
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
      return metrics::PlusAddressNetworkRequestTypeToString(info.param);
    });

TEST_F(PlusAddressHttpClientRequests,
       PreallocatePlusAddresses_IssuesCorrectRequest) {
  client().PreallocatePlusAddresses(base::DoNothing());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  EXPECT_EQ(last_request().url, kFullPreallocateEndpoint);
  EXPECT_EQ(last_request().method, net::HttpRequestHeaders::kPostMethod);
  EXPECT_EQ(
      last_request().headers.GetHeader("Authorization").value_or(std::string()),
      "Bearer " + kToken);

  // Validate the request payload.
  ASSERT_EQ(last_request().request_body, nullptr);
}

// Tests that a successful server response from the preallocation endpoint is
// received, parsed, and triggers the callback properly.
TEST_F(PlusAddressHttpClientRequests,
       PreallocatePlusAddresses_SuccessResponse) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<PlusAddressHttpClient::PreallocatePlusAddressesResult>
      future;
  client().PreallocatePlusAddresses(future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  const auto address1 = PreallocatedPlusAddress(PlusAddress("plus1@plus.com"),
                                                /*lifetime=*/base::Days(10));
  const auto address2 = PreallocatedPlusAddress(PlusAddress("plus2@foo.com"),
                                                /*lifetime=*/base::Days(50));

  const std::string json = test::MakePreallocateResponse({address1, address2});
  EXPECT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kFullPreallocateEndpoint, json));
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), std::vector({address1, address2}));

  histogram_tester.ExpectTotalCount(
      LatencyHistogramFor(PlusAddressNetworkRequestType::kPreallocate), 1);
  histogram_tester.ExpectUniqueSample(
      ResponseCodeHistogramFor(PlusAddressNetworkRequestType::kPreallocate),
      net::HTTP_OK, 1);
  histogram_tester.ExpectTotalCount(
      ResponseByteSizeHistogramFor(PlusAddressNetworkRequestType::kPreallocate),
      1);
}

// Tests that a malformed server response from the preallocation endpoint is
// received, parsed, and passes a parsing error to the callback.
TEST_F(PlusAddressHttpClientRequests, PreallocatePlusAddresses_ParsingError) {
  base::test::TestFuture<PlusAddressHttpClient::PreallocatePlusAddressesResult>
      future;
  client().PreallocatePlusAddresses(future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  EXPECT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kFullPreallocateEndpoint, "[]"));
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), base::unexpected(PlusAddressRequestError(
                              PlusAddressRequestErrorType::kParsingError)));
}

// Tests that a network error response from the preallocation endpoint is
// received and passed on to the callback.
TEST_F(PlusAddressHttpClientRequests, PreallocatePlusAddresses_NetworkError) {
  base::test::TestFuture<PlusAddressHttpClient::PreallocatePlusAddressesResult>
      future;
  client().PreallocatePlusAddresses(future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  EXPECT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kFullPreallocateEndpoint, "", net::HTTP_NOT_FOUND));
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(),
            base::unexpected(
                PlusAddressRequestError::AsNetworkError(net::HTTP_NOT_FOUND)));
}

// Tests that resetting the http client while a pre-allocate request is ongoing
// results in a `kUserSignedOut` error.
TEST_F(PlusAddressHttpClientRequests, PreallocatePlusAddresses_SignoutError) {
  base::test::TestFuture<PlusAddressHttpClient::PreallocatePlusAddressesResult>
      future;
  client().PreallocatePlusAddresses(future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  client().Reset();
  EXPECT_EQ(url_loader_factory().NumPending(), 0);
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), base::unexpected(PlusAddressRequestError(
                              PlusAddressRequestErrorType::kUserSignedOut)));
}

// Tests that calling reset cancels ongoing network requests and runs pending
// callbacks with a `PlusAddressRequestErrorType::kUserSignedOut`.
TEST_F(PlusAddressHttpClientRequests, ResetWhileWaitingForNetwork) {
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  base::test::TestFuture<const PlusProfileOrError&> future;

  client().ReservePlusAddress(origin, /*refresh=*/false, future.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kToken, base::Time::Max());

  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  client().Reset();
  EXPECT_EQ(url_loader_factory().NumPending(), 0);
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), base::unexpected(PlusAddressRequestError(
                              PlusAddressRequestErrorType::kUserSignedOut)));
}

// Tests that calling reset cancels ongoing OAuth requests and runs pending
// callbacks with a `PlusAddressRequestErrorType::kUserSignedOut`.
TEST_F(PlusAddressHttpClientRequests, ResetWhileWaitingForOAuth) {
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  std::string facet = origin.Serialize();
  base::test::TestFuture<const PlusProfileOrError&> future;

  client().ReservePlusAddress(origin, /*refresh=*/false, future.GetCallback());
  EXPECT_EQ(url_loader_factory().NumPending(), 0);
  client().Reset();
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), base::unexpected(PlusAddressRequestError(
                              PlusAddressRequestErrorType::kUserSignedOut)));
}

TEST(PlusAddressHttpClient, ChecksUrlParamIsValidGurl) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  std::string server_url = "https://foo.com/";
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressesEnabled,
      {{features::kEnterprisePlusAddressServerUrl.name, server_url}});
  PlusAddressHttpClientImpl client(
      identity_test_env.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_EQ(test_api(client).GetServerUrlForTesting().value_or(GURL()),
            server_url);
}

TEST(PlusAddressHttpClient, RejectsNonUrlStrings) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kPlusAddressesEnabled,
      {{features::kEnterprisePlusAddressServerUrl.name, "kirubeldotcom"}});
  PlusAddressHttpClientImpl client(
      identity_test_env.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_FALSE(test_api(client).GetServerUrlForTesting().has_value());
}

class PlusAddressAuthToken : public ::testing::Test {
 public:
  PlusAddressAuthToken() {
    // Init the feature param to add `kTestScope_` to GetUnconsentedOAuth2Scopes
    features_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled,
        {{features::kEnterprisePlusAddressOAuthScope.name, kTestScope}});
    InitClient();
  }

  static constexpr base::TimeDelta kTestTokenLifetime = base::Seconds(1000);
  static constexpr char kTestToken[] = "access_token";
  static constexpr char kTestScope[] = "https://googleapis.com/test.scope";

 protected:
  PlusAddressHttpClientImpl& client() { return *client_; }
  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  signin::IdentityManager* identity_manager() {
    return identity_env().identity_manager();
  }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void InitClient() {
    client_.emplace(identity_manager(), /*url_loader_factory=*/nullptr);
  }

  AccountInfo SignIn() {
    return identity_env().MakePrimaryAccountAvailable(
        "foo@gmail.com", signin::ConsentLevel::kSignin);
  }

  void WaitAndRespondToTokenRequest(base::Time expiration) {
    identity_env()
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            kTestToken, expiration, "unused", {kTestScope});
  }

 private:
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;

  std::optional<PlusAddressHttpClientImpl> client_;
};

TEST_F(PlusAddressAuthToken, RequestedBeforeSignin) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<std::optional<std::string>> callback;
  test_api(client()).GetAuthToken(callback.GetCallback());

  // The callback is run only after signin.
  EXPECT_FALSE(callback.IsReady());
  SignIn();
  WaitAndRespondToTokenRequest(base::Time::Now() + kTestTokenLifetime);

  EXPECT_TRUE(callback.Wait());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 1)));
}

TEST_F(PlusAddressAuthToken, RequestedUserNeverSignsIn) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<std::optional<std::string>> callback;
  test_api(client()).GetAuthToken(callback.GetCallback());
  EXPECT_FALSE(callback.IsReady());
  histogram_tester.ExpectTotalCount(kPlusAddressOauthErrorHistogram, 0);
}

TEST_F(PlusAddressAuthToken, RequestedAfterExpiration) {
  base::HistogramTester histogram_tester;
  // Make an initial OAuth token request.
  base::test::TestFuture<std::optional<std::string>> first_callback;
  test_api(client()).GetAuthToken(first_callback.GetCallback());

  // Sign in, get a token, and fast-forward to after it is expired.
  SignIn();
  WaitAndRespondToTokenRequest(base::Time::Now() + kTestTokenLifetime);
  EXPECT_TRUE(first_callback.Wait());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 1)));
  task_environment().FastForwardBy(kTestTokenLifetime + base::Seconds(1));

  // Issue another request for an OAuth token.
  base::test::TestFuture<std::optional<std::string>> second_callback;
  test_api(client()).GetAuthToken(second_callback.GetCallback());

  // Callback is only run once the new OAuth token request has completed.
  EXPECT_FALSE(second_callback.IsReady());
  WaitAndRespondToTokenRequest(base::Time::Now() + kTestTokenLifetime);
  EXPECT_TRUE(second_callback.Wait());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPlusAddressOauthErrorHistogram),
              BucketsAre(base::Bucket(GoogleServiceAuthError::State::NONE, 2)));
}

TEST_F(PlusAddressAuthToken, AuthErrorWithMultipleAccounts) {
  // GetAuthToken() is only concerned with the primary token auth state.
  AccountInfo primary = SignIn();
  AccountInfo secondary =
      identity_env().MakeAccountAvailable("secondary@foo.com");
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  InitClient();

  base::test::TestFuture<std::optional<std::string>> callback;
  test_api(client()).GetAuthToken(callback.GetCallback());
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary.account_id, kTestToken, base::Time::Max());
  EXPECT_EQ(callback.Get(), kTestToken);
}

TEST_F(PlusAddressAuthToken, RequestWorks_ManyCallers) {
  SignIn();

  // Issue several requests for an OAuth token.
  base::test::TestFuture<std::optional<std::string>> first;
  base::test::TestFuture<std::optional<std::string>> second;
  base::test::TestFuture<std::optional<std::string>> third;
  test_api(client()).GetAuthToken(first.GetCallback());
  test_api(client()).GetAuthToken(second.GetCallback());
  test_api(client()).GetAuthToken(third.GetCallback());

  // Although we failed to get a token, each callback should still be run.
  WaitAndRespondToTokenRequest(base::Time::Max());
  EXPECT_EQ(first.Get().value(), kTestToken);
  EXPECT_EQ(second.Get().value(), kTestToken);
  EXPECT_EQ(third.Get().value(), kTestToken);
}

TEST_F(PlusAddressAuthToken, RequestFails_ManyCallers) {
  SignIn();

  // Issue several requests for an OAuth token.
  base::test::TestFuture<std::optional<std::string>> first;
  base::test::TestFuture<std::optional<std::string>> second;
  base::test::TestFuture<std::optional<std::string>> third;
  test_api(client()).GetAuthToken(first.GetCallback());
  test_api(client()).GetAuthToken(second.GetCallback());
  test_api(client()).GetAuthToken(third.GetCallback());

  // Although we failed to get a token, each callback should still be run.
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(first.Get().has_value());
  EXPECT_FALSE(second.Get().has_value());
  EXPECT_FALSE(third.Get().has_value());
}

class PlusAddressHttpClientNullServerUrl : public PlusAddressHttpClientRequests {
 public:
  PlusAddressHttpClientNullServerUrl() {
    // Disable feature plus_addresses, which should also set `server_url_` to
    // `nullopt`.
    scoped_feature_list_.InitAndDisableFeature(features::kPlusAddressesEnabled);
    InitClient();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlusAddressHttpClientNullServerUrl, ReservePlusAddress_SendsNoRequest) {
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  base::test::TestFuture<const PlusProfileOrError&> callback;

  EXPECT_FALSE(test_api(client()).GetServerUrlForTesting().has_value());
  // ReservePlusAddress should return without making any request when no valid
  // `server_ur_` is provided.
  client().ReservePlusAddress(origin, /*refresh=*/false,
                              callback.GetCallback());
  EXPECT_EQ(url_loader_factory().NumPending(), 0);
  EXPECT_FALSE(callback.IsReady());
}

TEST_F(PlusAddressHttpClientNullServerUrl, ConfirmPlusAddress_SendsNoRequest) {
  const url::Origin origin = url::Origin::Create(GURL("https://foobar.com"));
  base::test::TestFuture<const PlusProfileOrError&> callback;

  EXPECT_FALSE(test_api(client()).GetServerUrlForTesting().has_value());
  // ConfirmPlusAddress should return without making any request when no valid
  // `server_ur_` is provided.
  client().ConfirmPlusAddress(origin, PlusAddress("foo@moo.com"),
                              callback.GetCallback());
  EXPECT_EQ(url_loader_factory().NumPending(), 0);
  EXPECT_FALSE(callback.IsReady());
}

}  // namespace
}  // namespace plus_addresses
