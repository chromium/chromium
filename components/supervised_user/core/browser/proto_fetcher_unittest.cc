// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher.h"

#include "stddef.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/test.pb.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace supervised_user {
namespace {

using ::base::BindOnce;
using ::base::Time;
using ::kids_chrome_management::ClassifyUrlRequest;
using ::kids_chrome_management::ClassifyUrlResponse;
using ::kids_chrome_management::CreatePermissionRequestResponse;
using ::kids_chrome_management::FamilyRole;
using ::kids_chrome_management::ListFamilyMembersRequest;
using ::kids_chrome_management::ListFamilyMembersResponse;
using ::kids_chrome_management::PermissionRequest;
using ::network::GetUploadData;
using ::network::TestURLLoaderFactory;
using ::signin::ConsentLevel;
using ::signin::IdentityTestEnvironment;

constexpr FetcherConfig kTestGetConfig{
    .service_path = "/superviser/user:get",
    // TODO(b/284523446): Refer to GaiaConstants rather than literal.
    .oauth2_scope =
        "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                           // required.
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "SupervisedUser.Request",
    .traffic_annotation =
        annotations::ClassifyUrlTag,  // traffic annotation is meaningless for
                                      // this tests since there's no real
                                      // traffic.
};

constexpr FetcherConfig kTestPostConfig{
    .service_path = "/superviser/user:post",
    // TODO(b/284523446): Refer to GaiaConstants rather than literal.
    .oauth2_scope =
        "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                           // required.
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "SupervisedUser.Request",
    .traffic_annotation =
        annotations::ClassifyUrlTag,  // traffic annotation is meaningless for
                                      // this tests since there's no real
                                      // traffic.
};

constexpr FetcherConfig kTestRetryConfig{
    .service_path = "/superviser/user:retry",
    // TODO(b/284523446): Refer to GaiaConstants rather than literal.
    .oauth2_scope =
        "https://www.googleapis.com/auth/kid.permission",  // Real scope
                                                           // required.
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
};

class Receiver {
 public:
  const base::expected<std::unique_ptr<Response>, ProtoFetcherStatus>&
  GetResult() const {
    return *result_;
  }
  bool HasResultOrError() const { return result_.has_value(); }

  void Receive(ProtoFetcherStatus fetch_status,
               std::unique_ptr<Response> response) {
    if (!fetch_status.IsOk()) {
      result_ = base::unexpected(fetch_status);
      return;
    }
    result_ = std::move(response);
  }

  void ReceiveDeferred(std::unique_ptr<DeferredProtoFetcher<Response>> fetcher,
                       ProtoFetcherStatus fetch_status,
                       std::unique_ptr<Response> response) {
    Receive(fetch_status, std::move(response));
  }

 private:
  absl::optional<base::expected<std::unique_ptr<Response>, ProtoFetcherStatus>>
      result_;
};

// Test fixture for proto fetcher.
// Defines required runtime environment, and a collection of helper methods
// which are used to build initial test state and define behaviours.
//
// Simulate* methods are short-hands to put response with specific property in
// test url environmnent's queue;
//
// FastForward is important for retrying feature tests: make sure that the time
// skipped is greater than possible retry timeouts.
class ProtoFetcherTest : public ::testing::TestWithParam<FetcherConfig> {
 protected:
  using Fetcher = DeferredProtoFetcher<Response>;

  void SetUp() override {
    SetHttpEndpointsForKidsManagementApis(feature_list_, "example.com");
  }

  const FetcherConfig& GetConfig() const { return GetParam(); }

  // Receivers are not-copyable because of mocked method.
  std::unique_ptr<Receiver> MakeReceiver() const {
    return std::make_unique<Receiver>();
  }
  std::unique_ptr<Fetcher> MakeFetcher(Receiver& receiver) {
    std::unique_ptr<Fetcher> fetcher = CreateTestFetcher(
        *identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper(), Request(), GetConfig());
    fetcher->Start(BindOnce(&Receiver::Receive, base::Unretained(&receiver)));
    return fetcher;
  }

  const GURL& GetUrlOfPendingRequest(size_t index) {
    return test_url_loader_factory_.GetPendingRequest(index)->request.url;
  }

  void SimulateDefaultResponseForPendingRequest(size_t index) {
    Response response;
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetUrlOfPendingRequest(index).spec(), response.SerializeAsString());
  }
  void SimulateResponseForPendingRequest(size_t index,
                                         base::StringPiece content) {
    Response response;
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
  void SimulateResponseForPendingRequestWithPersistentError(size_t index) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetUrlOfPendingRequest(index).spec(), "malformed-response");
  }

  void FastForward() {
    // Fast forward enough to schedule all retries, which for testing should be
    // configured as order of millisecond.
    task_environment_.FastForwardBy(base::Hours(1));
  }

  void MakePrimaryAccountAvailable() {
    identity_test_env_.MakePrimaryAccountAvailable("bob@gmail.com",
                                                   ConsentLevel::kSignin);
  }
  void SetAutomaticIssueOfAccessTokens() {
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

 private:
  // Must be first attribute, see base::test::TaskEnvironment docs.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ProtoFetcherTest, ConfiguresEndpoint) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  GURL expected_url =
      GURL("http://example.com" + std::string(GetConfig().service_path) +
           "?alt=proto");
  EXPECT_EQ(pending_request->request.url, expected_url);
  EXPECT_EQ(pending_request->request.method, GetConfig().GetHttpMethod());
}

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

  std::string header;
  EXPECT_TRUE(pending_request->request.headers.GetHeader(
      net::HttpRequestHeaders::kContentType, &header));
  EXPECT_EQ(header, "application/x-protobuf");
}

TEST_P(ProtoFetcherTest, AcceptsRequests) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateDefaultResponseForPendingRequest(0);

  EXPECT_TRUE(receiver->GetResult().has_value());
}

TEST_P(ProtoFetcherTest, NoAccessToken) {
  MakePrimaryAccountAvailable();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(receiver->GetResult().error().state(),
            ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR);
  EXPECT_EQ(receiver->GetResult().error().google_service_auth_error().state(),
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
}

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

TEST_P(ProtoFetcherTest, CreatesToken) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

  // That's enough: request is pending, so token is accepted.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Only check header format here.
  std::string authorization_header;
  ASSERT_TRUE(
      test_url_loader_factory_.GetPendingRequest(0)->request.headers.GetHeader(
          net::HttpRequestHeaders::kAuthorization, &authorization_header));
  EXPECT_EQ(authorization_header, "Bearer access_token");
}

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

TEST_P(ProtoFetcherTest, RecordsMetrics) {
  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());
  base::HistogramTester histogram_tester;

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateDefaultResponseForPendingRequest(0);

  ASSERT_TRUE(receiver->GetResult().has_value());

  // The actual latency of mocked fetch is variable, so only expect that some
  // value was recorded.
  histogram_tester.ExpectTotalCount(
      base::StrCat({GetConfig().histogram_basename, ".Latency"}),
      /*expected_count(grew by)*/ 1);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          base::StrCat({GetConfig().histogram_basename, ".Status"})),
      base::BucketsInclude(base::Bucket(ProtoFetcherStatus::State::OK, 1)));

  // Tests that no enum above ::OK was emitted:
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  base::StrCat({GetConfig().histogram_basename, ".Status"})),
              base::BucketsInclude(base::Bucket(
                  ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR, 0)));
}

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

  // Expect that one sample with value 3 (number of requests) was recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                  {GetConfig().histogram_basename, ".RetryCount"})),
              base::BucketsInclude(base::Bucket(3, 1)));
}

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
  SimulateResponseForPendingRequestWithPersistentError(0);
  FastForward();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 0);

  EXPECT_TRUE(receiver->HasResultOrError());
  EXPECT_TRUE(receiver->GetResult().error().IsPersistentError());

  // Expect that one sample with value 2 (number of requests) was recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                  {GetConfig().histogram_basename, ".RetryCount"})),
              base::BucketsInclude(base::Bucket(2, 1)));
}

TEST_P(ProtoFetcherTest, RetryingFetcherContinuesOnTransientError) {
  if (!GetConfig().backoff_policy.has_value()) {
    GTEST_SKIP() << "Tests retrying features.";
  }

  MakePrimaryAccountAvailable();
  SetAutomaticIssueOfAccessTokens();
  std::unique_ptr<Receiver> receiver = MakeReceiver();
  std::unique_ptr<Fetcher> fetcher = MakeFetcher(*receiver.get());

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
}

// Instead of /0, /1... print human-readable description of the test: status of
// the retrying feature followed by http method.
std::string PrettyPrintFetcherTestCaseName(
    const ::testing::TestParamInfo<FetcherConfig>& info) {
  return base::StrCat(
      {(info.param.backoff_policy.has_value() ? "Retrying" : ""),
       info.param.GetHttpMethod()});
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProtoFetcherTest,
                         testing::Values(kTestGetConfig,
                                         kTestPostConfig,
                                         kTestRetryConfig),
                         &PrettyPrintFetcherTestCaseName);

class FetchManagerTest : public testing::Test {
 public:
  MOCK_METHOD2(Done,
               void(ProtoFetcherStatus, std::unique_ptr<ClassifyUrlResponse>));

 protected:
  void SetUp() override {
    // Fetch process is two-phase (access token and then rpc). The test flow
    // will be controlled by releasing pending requests.
    identity_test_env_.MakePrimaryAccountAvailable("bob@gmail.com",
                                                   ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  // Flips order of arguments so that the sole unbound argument will be the
  // request.
  static std::unique_ptr<DeferredProtoFetcher<ClassifyUrlResponse>> ClassifyURL(
      signin::IdentityManager* identity_manager,
      network::TestURLLoaderFactory& url_loader_factory,
      const FetcherConfig& config,
      const ClassifyUrlRequest& request) {
    return supervised_user::CreateClassifyURLFetcher(
        *identity_manager, url_loader_factory.GetSafeWeakWrapper(), request,
        config);
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
  IdentityTestEnvironment identity_test_env_;

  base::RepeatingCallback<std::unique_ptr<
      DeferredProtoFetcher<ClassifyUrlResponse>>(const ClassifyUrlRequest&)>
      factory_{base::BindRepeating(&FetchManagerTest::ClassifyURL,
                                   identity_test_env_.identity_manager(),
                                   std::ref(test_url_loader_factory_),
                                   kClassifyUrlConfig)};
  ClassifyUrlRequest request_;
  ClassifyUrlResponse response_;
};

TEST_F(FetchManagerTest, HandlesMultipleRequests) {
  EXPECT_CALL(*this, Done(::testing::_, ::testing::_)).Times(2);

  ParallelFetchManager<ClassifyUrlRequest, ClassifyUrlResponse> under_test(
      factory_);

  under_test.Fetch(request_, base::BindOnce(&FetchManagerTest::Done,
                                            base::Unretained(this)));
  under_test.Fetch(request_, base::BindOnce(&FetchManagerTest::Done,
                                            base::Unretained(this)));

  // task_environment_.RunUntilIdle() would be called from simulations.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 2L);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
}

TEST_F(FetchManagerTest, CancelsRequestsUponDestruction) {
  EXPECT_CALL(*this, Done(::testing::_, ::testing::_)).Times(0);

  {
    ParallelFetchManager<ClassifyUrlRequest, ClassifyUrlResponse> under_test(
        factory_);
    under_test.Fetch(request_, base::BindOnce(&FetchManagerTest::Done,
                                              base::Unretained(this)));
    under_test.Fetch(request_, base::BindOnce(&FetchManagerTest::Done,
                                              base::Unretained(this)));

    // Callbacks are pending on blocked network traffic.
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 2L);

    // Now under_test will go out of scope.
  }

  // Unblocking network traffic won't help executing callbacks, since their
  // parent manager |under_test| is now gone.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
}

class DeferredFetcherTest : public ::testing::Test {
 protected:
  using CallbackType = void(ProtoFetcherStatus,
                            std::unique_ptr<CreatePermissionRequestResponse>);

 public:
  MOCK_METHOD2(Done, CallbackType);

 protected:
  void SetUp() override {
    // Fetch process is two-phase (access token and then rpc). The test flow
    // will be controlled by releasing pending requests.
    identity_test_env_.MakePrimaryAccountAvailable("bob@gmail.com",
                                                   ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  // Used to demonstrate DeferredProtoFetcher anit-pattern.
  static void OnResponse(
      std::unique_ptr<DeferredProtoFetcher<CreatePermissionRequestResponse>>
          fetcher,
      base::OnceCallback<CallbackType> callback,
      ProtoFetcherStatus status,
      std::unique_ptr<CreatePermissionRequestResponse> response) {
    std::move(callback).Run(status, std::move(response));
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
  IdentityTestEnvironment identity_test_env_;
};

TEST_F(DeferredFetcherTest, IsCreatedAndStarted) {
  EXPECT_CALL(*this, Done(::testing::_, ::testing::_)).Times(1);

  {
    // Putting the following code in separate scope demonstrates that this
    // fetcher survives going out-of-scope, because it is bound to the callback
    // which is in turn referenced in the task environment. Outside of this
    // scope, there is no way to cancel this fetcher.
    std::unique_ptr<DeferredProtoFetcher<CreatePermissionRequestResponse>>
        fetcher = CreatePermissionRequestFetcher(
            *identity_test_env_.identity_manager(),
            test_url_loader_factory_.GetSafeWeakWrapper(),
            // Payload does not matter, not validated on client side.
            PermissionRequest());

    base::OnceCallback<CallbackType> callback =
        base::BindOnce(&DeferredFetcherTest::Done, base::Unretained(this));

    // Self-bind pattern.
    auto* fetcher_ptr = fetcher.get();
    fetcher_ptr->Start(
        base::BindOnce(&OnResponse, std::move(fetcher), std::move(callback)));

    // Reference to the fetcher is already lost (last chance in fetcher_ptr
    // which will soon go out-of-scope without destroying the fetcher).
    ASSERT_TRUE(!fetcher);
  }

  // Callbacks are pending on blocked network traffic.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1L);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      CreatePermissionRequestResponse().SerializeAsString());
}

}  // namespace
}  // namespace supervised_user
