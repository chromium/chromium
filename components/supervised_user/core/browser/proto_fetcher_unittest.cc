// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "google_apis/common/api_key_request_test_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

bool operator==(const ProtoFetcherStatus& a, const ProtoFetcherStatus& b) {
  return a.state() == b.state();
}

namespace {

static const char* kMockEndpointDomain = "example.com";
static const char* kExampleURL = "https://example.com/";

using ::base::BindOnce;
using ::base::Time;
using ::kidsmanagement::ClassifyUrlRequest;
using ::kidsmanagement::ClassifyUrlResponse;
using ::kidsmanagement::CreatePermissionRequestResponse;
using ::kidsmanagement::FamilyRole;
using ::kidsmanagement::PermissionRequest;
using ::network::GetUploadData;
using ::network::TestURLLoaderFactory;
using ::signin::ConsentLevel;
using ::signin::IdentityTestEnvironment;

constexpr FetcherConfig kTestGetConfig{
    .service_path = "/superviser/user:get",
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "SupervisedUser.Request",
    .traffic_annotation =
        annotations::ClassifyUrlTag,  // traffic annotation is meaningless for
                                      // this tests since there's no real
                                      // traffic.
    .access_token_config{
        .credentials_requirement =
            AccessTokenConfig::CredentialsRequirement::kStrict,
        .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope =
            "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                               // required.

    },
};

constexpr FetcherConfig kTestGetConfigWithoutMetrics{
    .service_path = "/superviser/user:get",
    .method = FetcherConfig::Method::kGet,
    .traffic_annotation =
        annotations::ClassifyUrlTag,  // traffic annotation is meaningless for
                                      // this tests since there's no real
                                      // traffic.
    .access_token_config{
        .credentials_requirement =
            AccessTokenConfig::CredentialsRequirement::kStrict,
        .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope =
            "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                               // required.

    },
};

constexpr FetcherConfig kTestPostConfig{
    .service_path = "/superviser/user:post",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "SupervisedUser.Request",
    .traffic_annotation =
        annotations::ClassifyUrlTag,  // traffic annotation is meaningless for
                                      // this tests since there's no real
                                      // traffic.
    .access_token_config{
        .credentials_requirement =
            AccessTokenConfig::CredentialsRequirement::kStrict,
        .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope =
            "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                               // required.

    },
};

constexpr FetcherConfig kTestRetryConfig{
    .service_path = "/superviser/user:retry",
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "SupervisedUser.Request",
    .traffic_annotation =
        annotations::ClassifyUrlTag,  // traffic annotation is meaningless for
                                      // this tests since there's no real
                                      // traffic.
    .backoff_policy =
        net::BackoffEntry::Policy{
            .initial_delay_ms = 1,
            .multiply_factor = 1,
            .maximum_backoff_ms = 1,
            .always_use_initial_delay = false,
        },
    .access_token_config{
        .credentials_requirement =
            AccessTokenConfig::CredentialsRequirement::kStrict,
        .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope =
            "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                               // required.

    },
};

constexpr FetcherConfig kTestGetConfigBestEffortAccessToken{
    .service_path = "/superviser/user:get",
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "SupervisedUser.Request",
    .traffic_annotation =
        annotations::ClassifyUrlTag,  // traffic annotation is meaningless for
                                      // this tests since there's no real
                                      // traffic.
    .access_token_config{
        .credentials_requirement =
            AccessTokenConfig::CredentialsRequirement::kBestEffort,
        .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope =
            "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                               // required.

    },
};

// Receiver is an artificial consumer of the fetch process. Typically, calling
// an RPC has the purpose of writing the data somewhere. Instances of this class
// serve as a general-purpose destination for fetched data.
class Receiver {
 public:
  const base::expected<std::unique_ptr<ClassifyUrlResponse>,
                       ProtoFetcherStatus>&
  GetResult() const {
    return *result_;
  }
  bool HasResultOrError() const { return result_.has_value(); }

  void Receive(const ProtoFetcherStatus& fetch_status,
               std::unique_ptr<ClassifyUrlResponse> response) {
    if (!fetch_status.IsOk()) {
      result_ = base::unexpected(fetch_status);
      return;
    }
    result_ = std::move(response);
  }

 private:
  std::optional<
      base::expected<std::unique_ptr<ClassifyUrlResponse>, ProtoFetcherStatus>>
      result_;
};

// Base of the test fixture for proto fetcher.
// Defines required runtime environment, and a collection of helper methods
// which are used to build initial test state and define behaviours.
//
// Simulate* methods are short-hands to put response with specific property in
// test url environmnent's queue;
//
// FastForward is important for retrying feature tests: make sure that the time
// skipped is greater than possible retry timeouts.
class ProtoFetcherTestBase {
 public:
  ProtoFetcherTestBase() = delete;
  explicit ProtoFetcherTestBase(const FetcherConfig& config) : config_(config) {
    SetHttpEndpointsForKidsManagementApis(feature_list_, kMockEndpointDomain);
  }

 protected:
  using Fetcher = ProtoFetcher<ClassifyUrlResponse>;

  const FetcherConfig& GetConfig() const { return config_; }

  // Receivers are not-copyable because of mocked method.
  std::unique_ptr<Receiver> MakeReceiver() const {
    return std::make_unique<Receiver>();
  }
  std::unique_ptr<Fetcher> MakeFetcher(Receiver& receiver) {
    ClassifyUrlRequest request;
    if (GetConfig().method == FetcherConfig::Method::kPost) {
      request.set_url(kExampleURL);
    }
    return CreateClassifyURLFetcher(
        *identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper(), request,
        BindOnce(&Receiver::Receive, base::Unretained(&receiver)), GetConfig());
  }

  // Url loader helpers
  const GURL& GetUrlOfPendingRequest(size_t index) {
    return test_url_loader_factory_.GetPendingRequest(index)->request.url;
  }

  void SimulateDefaultResponseForPendingRequest(size_t index) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetUrlOfPendingRequest(index).spec(),
        ClassifyUrlResponse().SerializeAsString());
  }
  void SimulateResponseForPendingRequest(size_t index,
                                         std::string_view content) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetUrlOfPendingRequest(index).spec(), std::string(content));
  }
  void SimulateResponseForPendingRequestWithTransientError(size_t index) {
    net::HttpStatusCode error = net::HTTP_BAD_REQUEST;
    ASSERT_TRUE(
        ProtoFetcherStatus::HttpStatusOrNetError(error).IsTransientError());

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetUrlOfPendingRequest(index).spec(), /*content=*/"", error);
  }
  void SimulateMalformedResponseForPendingRequest(size_t index) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetUrlOfPendingRequest(index).spec(), "malformed-response");
  }
  void SimulateResponseForPendingRequestWithHttpAuthError(size_t index) {
    net::HttpStatusCode error = net::HTTP_UNAUTHORIZED;
    ASSERT_FALSE(
        ProtoFetcherStatus::HttpStatusOrNetError(error).IsTransientError());

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetUrlOfPendingRequest(index).spec(), /*content=*/"", error);
  }

  // Some tests check backoff strategies which introduce delays, this method is
  // forwarding the time so that all operations scheduled in the future are
  // completed. See FetcherConfig::backoff_policy for details.
  void FastForward() {
    // Fast forward enough to schedule all retries, which for testing should be
    // configured as order of millisecond.
    task_environment_.FastForwardBy(base::Hours(1));
  }

  // Test identity environment helpers.
  void MakePrimaryAccountAvailable() {
    identity_test_env_.MakePrimaryAccountAvailable("bob@gmail.com",
                                                   ConsentLevel::kSignin);
  }
  void SetAutomaticIssueOfAccessTokens() {
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  bool MetricsAreExpected() const {
    return GetConfig().histogram_basename.has_value();
  }

 private:
  // Must be first attribute, see base::test::TaskEnvironment docs.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FetcherConfig config_;

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList feature_list_;
};

struct ProtoFetcherTestParam {
  using TupleT = std::tuple<FetcherConfig, bool>;
  FetcherConfig fetcher_config;
  bool uncredentialed_fallback_enabled;
  explicit ProtoFetcherTestParam(TupleT t)
      : fetcher_config(std::get<0>(t)),
        uncredentialed_fallback_enabled(std::get<1>(t)) {}
};

class ProtoFetcherTest
    : public ProtoFetcherTestBase,
      public ::testing::TestWithParam<ProtoFetcherTestParam> {
 public:
  ProtoFetcherTest() : ProtoFetcherTestBase(GetConfig()) {
    feature_list_.InitWithFeatureState(
        supervised_user::kUncredentialedFilteringFallbackForSupervisedUsers,
        UncredentialedFallbackEnabled());
  }
  static const FetcherConfig& GetConfig() { return GetParam().fetcher_config; }
  static bool UncredentialedFallbackEnabled() {
    return GetParam().uncredentialed_fallback_enabled;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test whether the outgoing request has correctly set endpoint and method.
TEST_P(ProtoFetcherTest, ConfiguresEndpoint) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  GURL expected_url =
      GURL("http://" + std::string(kMockEndpointDomain) +
           std::string(GetConfig().StaticServicePath()) + "?alt=proto");
  EXPECT_EQ(pending_request->request.url, expected_url);
  EXPECT_EQ(pending_request->request.method, GetConfig().GetHttpMethod());
}

// Test whether the outgoing request has the HTTP payload, only for those HTTP
// verbs that support it.
TEST_P(ProtoFetcherTest, AddsPayload) {
  if (GetConfig().method != FetcherConfig::Method::kPost) {
    GTEST_SKIP() << "Payload not supported for " << GetConfig().GetHttpMethod()
                 << " requests.";
  }

  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  EXPECT_EQ(pending_request->request.headers.GetHeader(
                net::HttpRequestHeaders::kContentType),
            "application/x-protobuf");
}

// Tests a default flow, where an empty (default) proto is received.
TEST_P(ProtoFetcherTest, AcceptsRequests) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateDefaultResponseForPendingRequest(0);

  EXPECT_TRUE(receiver->GetResult().has_value());
}

// Tests a flow where the caller is denied access token. There should be
// response consumed, that indicated auth error and contains details about the
// reason for denying access.
TEST_P(ProtoFetcherTest, NoAccessTokenStrict) {
  MakePrimaryAccountAvailable();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  base::HistogramTester histogram_tester;

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(receiver->GetResult().error().state(),
            ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR);

  if (MetricsAreExpected()) {
    // This tests just the metrics related to the auth error case; the rest of
    // the metrics are tested in other tests.
    histogram_tester.ExpectUniqueSample(
        base::StrCat({*GetConfig().histogram_basename, ".Status"}),
        ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR, 1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat({*GetConfig().histogram_basename, ".AuthError"}),
        GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS, 1);
  }
}

// Tests a flow where incoming data from RPC can't be deserialized to a valid
// proto.
TEST_P(ProtoFetcherTest, HandlesMalformedResponse) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequest(0, "malformed-value");

  EXPECT_FALSE(receiver->GetResult().has_value());
  EXPECT_EQ(receiver->GetResult().error().state(),
            ProtoFetcherStatus::State::INVALID_RESPONSE);
}

// Tests whether access token information is added to the request in a right
// header.
TEST_P(ProtoFetcherTest, CreatesToken) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  // That's enough: request is pending, so token is accepted.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Only check header format here.
  EXPECT_EQ(
      test_url_loader_factory_.GetPendingRequest(0)->request.headers.GetHeader(
          net::HttpRequestHeaders::kAuthorization),
      "Bearer access_token");
}

// Tests a flow where the request couldn't be completed due to network
// infrastructure errors. The result must contain details about the error.
TEST_P(ProtoFetcherTest, HandlesNetworkError) {
  if (GetConfig().backoff_policy.has_value()) {
    GTEST_SKIP() << "Test not suitable for retrying fetchers: is serves "
                    "transient errors which do not produce results.";
  }

  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetUrlOfPendingRequest(0),
      network::URLLoaderCompletionStatus(net::ERR_UNEXPECTED),
      network::CreateURLResponseHead(net::HTTP_OK), /*content=*/"");
  EXPECT_FALSE(receiver->GetResult().has_value());
  EXPECT_EQ(receiver->GetResult().error().state(),
            ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  EXPECT_EQ(receiver->GetResult().error().http_status_or_net_error(),
            ProtoFetcherStatus::HttpStatusOrNetErrorType(net::ERR_UNEXPECTED));
}

// Tests a flow where the remote server couldn't process the request and
// responded with an error.
TEST_P(ProtoFetcherTest, HandlesServerError) {
  if (GetConfig().backoff_policy.has_value()) {
    GTEST_SKIP() << "Test not suitable for retrying fetchers: is serves "
                    "transient errors which do not produce results.";
  }

  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetUrlOfPendingRequest(0).spec(), /*content=*/"", net::HTTP_BAD_REQUEST);

  EXPECT_FALSE(receiver->GetResult().has_value());
  EXPECT_EQ(receiver->GetResult().error().state(),
            ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  EXPECT_EQ(
      receiver->GetResult().error().http_status_or_net_error(),
      ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_BAD_REQUEST));
}

// Tests a flow where the remote server responds with an auth error (401).
TEST_P(ProtoFetcherTest, HandlesRepeatedServerAuthError) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequestWithHttpAuthError(0);

  if (UncredentialedFallbackEnabled()) {
    // This triggers a retry of the request. The access token is invalidated
    // and a new one is fetched.
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate another 401 response from the server (which will not be
    // retried).
    SimulateResponseForPendingRequestWithHttpAuthError(0);
  }

  // The request is failed.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_FALSE(receiver->GetResult().has_value());
  EXPECT_EQ(receiver->GetResult().error().state(),
            ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  EXPECT_EQ(
      receiver->GetResult().error().http_status_or_net_error(),
      ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_UNAUTHORIZED));
}

// Tests a flow where the remote server responds with an auth error (401), and
// then the request succeeds on retry.
TEST_P(ProtoFetcherTest, HandlesServerAuthErrorThenSuccess) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequestWithHttpAuthError(0);

  if (UncredentialedFallbackEnabled()) {
    // This triggers a retry of the request. The access token is invalidated
    // and a new one is fetched.
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

    // This time the server responds with success.
    SimulateDefaultResponseForPendingRequest(0);

    ASSERT_TRUE(receiver->GetResult().has_value());
  } else {
    // The request is failed.
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 0);
    EXPECT_FALSE(receiver->GetResult().has_value());
    EXPECT_EQ(receiver->GetResult().error().state(),
              ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
    EXPECT_EQ(
        receiver->GetResult().error().http_status_or_net_error(),
        ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_UNAUTHORIZED));
  }
}

// The fetchers are recording various metrics for the basic flow with default
// empty proto response. This test is checking whether all metrics receive
// right values.
TEST_P(ProtoFetcherTest, RecordsMetrics) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());
  base::HistogramTester histogram_tester;

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateDefaultResponseForPendingRequest(0);

  ASSERT_TRUE(receiver->GetResult().has_value());

  if (MetricsAreExpected()) {
    // The actual latency of mocked fetch is variable, so only expect that
    // some value was recorded.
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".Latency"}),
        /*expected_count(grew by)*/ 1);
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".NoError.Latency"}),
        /*expected_count(grew by)*/ 1);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            base::StrCat({*GetConfig().histogram_basename, ".Status"})),
        base::BucketsInclude(
            base::Bucket(ProtoFetcherStatus::State::OK, /*count=*/1),
            base::Bucket(ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR,
                         /*count=*/0)));
  }
}

// When retrying is configured, the fetch process is re-launched until a
// decisive status is received (OK or permanent error, see
// RetryingFetcherImpl::ShouldRetry for details). This tests checks that the
// compound fetch process eventually terminates and that related metrics are
// also recorded.
TEST_P(ProtoFetcherTest, RetryingFetcherTerminatesOnOkStatusAndRecordsMetrics) {
  if (!GetConfig().backoff_policy.has_value()) {
    GTEST_SKIP() << "Tests retrying features.";
  }

  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  base::HistogramTester histogram_tester;

  // First transient errors.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequestWithTransientError(0);
  FastForward();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequestWithTransientError(0);
  FastForward();

  // Then success.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateDefaultResponseForPendingRequest(0);
  FastForward();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 0);

  EXPECT_TRUE(receiver->HasResultOrError());
  EXPECT_TRUE(receiver->GetResult().has_value());

  if (MetricsAreExpected()) {
    // Expect that one sample with value 3 (number of requests) was recorded.
    EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                    {*GetConfig().histogram_basename, ".RetryCount"})),
                base::BucketsInclude(base::Bucket(3, 1)));

    // The actual latency of mocked fetch is variable, so only expect that
    // some value was recorded.
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".OverallLatency"}),
        /*expected_count(grew by)*/ 1);

    EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                    {*GetConfig().histogram_basename, ".OverallStatus"})),
                base::BucketsInclude(
                    base::Bucket(ProtoFetcherStatus::State::OK, /*count=*/1)));

    // Individual status and latencies were also recorded because the compound
    // fetcher consists of an individual fetchers. Note that the count of
    // individual metrics grew by the number related to number of responses
    // used.

    // The actual latency of mocked fetch is variable, so only expect that
    // some value was recorded.
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".Latency"}),
        /*expected_count(grew by)*/ 3);
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".NoError.Latency"}),
        /*expected_count(grew by)*/ 1);

    histogram_tester.ExpectTotalCount(
        base::StrCat(
            {*GetConfig().histogram_basename, ".HttpStatusOrNetError.Latency"}),
        /*expected_count(grew by)*/ 2);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            base::StrCat({*GetConfig().histogram_basename, ".Status"})),
        base::BucketsInclude(
            base::Bucket(ProtoFetcherStatus::State::OK, /*count=*/1),
            base::Bucket(ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR,
                         /*count=*/2)));
  }
}

// When retrying is configured, the fetch process is re-launched until a
// decisive status is received (OK or permanent error, see
// RetryingFetcherImpl::ShouldRetry for details). This tests checks that the
// compound fetch process eventually terminates and that related metrics are
// also recorded.
TEST_P(ProtoFetcherTest,
       RetryingFetcherTerminatesOnPersistentErrorAndRecordsMetrics) {
  if (!GetConfig().backoff_policy.has_value()) {
    GTEST_SKIP() << "Tests retrying features.";
  }

  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  base::HistogramTester histogram_tester;

  // First transient error.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequestWithTransientError(0);
  FastForward();

  // Then persistent error.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateMalformedResponseForPendingRequest(0);
  FastForward();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 0);

  EXPECT_TRUE(receiver->HasResultOrError());
  EXPECT_TRUE(receiver->GetResult().error().IsPersistentError());

  if (MetricsAreExpected()) {
    // Expect that one sample with value 2 (number of requests) was recorded.
    EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                    {*GetConfig().histogram_basename, ".RetryCount"})),
                base::BucketsInclude(base::Bucket(2, 1)));

    // The actual latency of mocked fetch is variable, so only expect that
    // some value was recorded.
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".OverallLatency"}),
        /*expected_count(grew by)*/ 1);

    EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                    {*GetConfig().histogram_basename, ".OverallStatus"})),
                base::BucketsInclude(base::Bucket(
                    ProtoFetcherStatus::State::INVALID_RESPONSE, 1)));

    // Individual status and latencies were also recorded because the compound
    // fetcher consists of an individual fetchers. Note that the count of
    // individual metrics grew by the number related to number of responses
    // used.

    // The actual latency of mocked fetch is variable, so only expect that
    // some value was recorded.
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".Latency"}),
        /*expected_count(grew by)*/ 2);
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".ParseError.Latency"}),
        /*expected_count(grew by)*/ 1);

    histogram_tester.ExpectTotalCount(
        base::StrCat(
            {*GetConfig().histogram_basename, ".HttpStatusOrNetError.Latency"}),
        /*expected_count(grew by)*/ 1);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            base::StrCat({*GetConfig().histogram_basename, ".Status"})),
        base::BucketsInclude(
            base::Bucket(ProtoFetcherStatus::State::INVALID_RESPONSE,
                         /*count=*/1),
            base::Bucket(ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR,
                         /*count=*/1)));
  }
}

// When retrying is configured, the fetch process is re-launched until a
// decisive status is received (OK or permanent error, see
// RetryingFetcherImpl::ShouldRetry for details). This tests assumes only
// transient error responses from the server (eg. those that are expect to go
// away on their own soon). This means that no response will be received, and
// no extra retrying metrics recording, because the process is still not
// finished.
TEST_P(ProtoFetcherTest, RetryingFetcherContinuesOnTransientError) {
  if (!GetConfig().backoff_policy.has_value()) {
    GTEST_SKIP() << "Tests retrying features.";
  }

  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  base::HistogramTester histogram_tester;

  // Only transient errors.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequestWithTransientError(0);
  FastForward();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateResponseForPendingRequestWithTransientError(0);
  FastForward();

  // Request is still pending, because the system keeps retrying.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_FALSE(receiver->HasResultOrError());

  if (MetricsAreExpected()) {
    // No final status was recorded as the fetcher is still pending.
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".OverallStatus"}),
        /*expected_count(grew by)*/ 0);

    // Individual status and latencies were also recorded because the compound
    // fetcher consists of an individual fetchers. Note that the count of
    // individual metrics grew by the number related to number of responses
    // used.

    // The actual latency of mocked fetch is variable, so only expect that
    // some value was recorded.
    histogram_tester.ExpectTotalCount(
        base::StrCat({*GetConfig().histogram_basename, ".Latency"}),
        /*expected_count(grew by)*/ 2);
    histogram_tester.ExpectTotalCount(
        base::StrCat(
            {*GetConfig().histogram_basename, ".HttpStatusOrNetError.Latency"}),
        /*expected_count(grew by)*/ 2);

    EXPECT_THAT(histogram_tester.GetAllSamples(
                    base::StrCat({*GetConfig().histogram_basename, ".Status"})),
                base::BucketsInclude(base::Bucket(
                    ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR,
                    /*count=*/2)));
  }
}

// Instead of /0, /1... print human-readable description of the test: status
// of the retrying feature followed by http method.
std::string PrettyPrintFetcherTestCaseName(
    const ::testing::TestParamInfo<ProtoFetcherTestParam>& info) {
  const FetcherConfig& fetcher_config = info.param.fetcher_config;
  std::string base = fetcher_config.GetHttpMethod();
  if (fetcher_config.backoff_policy.has_value()) {
    base += "Retrying";
  }
  if (fetcher_config.histogram_basename.has_value()) {
    base += "WithMetrics";
  } else {
    base += "WithoutMetrics";
  }

  if (info.param.uncredentialed_fallback_enabled) {
    base += "_FallbackEnabled";
  } else {
    base += "_FallbackDisabled";
  }

  return base;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProtoFetcherTest,
    testing::ConvertGenerator<ProtoFetcherTestParam::TupleT>(
        testing::Combine(testing::Values(kTestGetConfig,
                                         kTestPostConfig,
                                         kTestRetryConfig,
                                         kTestGetConfigWithoutMetrics),
                         testing::Bool())),
    &PrettyPrintFetcherTestCaseName);

class BestEffortProtoFetcherTest : public ProtoFetcherTestBase,
                                   public testing::Test {
 public:
  BestEffortProtoFetcherTest()
      : ProtoFetcherTestBase(GetConfig()),
        feature_list_(supervised_user::
                          kUncredentialedFilteringFallbackForSupervisedUsers) {}
  static const FetcherConfig& GetConfig() {
    return kTestGetConfigBestEffortAccessToken;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests a flow where the caller is denied access token.
//
// Since the fetcher config specifies best effort as the credentials
// requirement, the request proceeds.
TEST_F(BestEffortProtoFetcherTest, NoAccessToken) {
  MakePrimaryAccountAvailable();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  base::HistogramTester histogram_tester;

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  ASSERT_TRUE(google_apis::test_util::HasAPIKey(
      test_url_loader_factory_.GetPendingRequest(0)->request));

  SimulateDefaultResponseForPendingRequest(0);

  EXPECT_TRUE(receiver->GetResult().has_value());

  // Check that both the overall Status, and the detailed AuthError metrics
  // are recorded.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          base::StrCat({*GetConfig().histogram_basename, ".Status"})),
      base::BucketsInclude(
          base::Bucket(ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR,
                       /*count=*/1),
          base::Bucket(ProtoFetcherStatus::State::OK,
                       /*count=*/1)));

  histogram_tester.ExpectUniqueSample(
      base::StrCat({*GetConfig().histogram_basename, ".AuthError"}),
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS, 1);
}

// Tests a flow where the caller is denied access token.
//
// Since the fetcher config specifies best effort as the credentials
// requirement, the request proceeds.
TEST_F(BestEffortProtoFetcherTest, RetryAfterHttpAuthError) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  ASSERT_FALSE(google_apis::test_util::HasAPIKey(
      test_url_loader_factory_.GetPendingRequest(0)->request));

  SimulateResponseForPendingRequestWithHttpAuthError(0);

  // The request is retried without EUC.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  ASSERT_TRUE(google_apis::test_util::HasAPIKey(
      test_url_loader_factory_.GetPendingRequest(0)->request));

  // The server responds OK.
  SimulateDefaultResponseForPendingRequest(0);

  // The response is successful.
  EXPECT_TRUE(receiver->GetResult().has_value());
}

}  // namespace
}  // namespace supervised_user
