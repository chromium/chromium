// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/fetcher_config_test_utils.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "google_apis/gaia/google_service_auth_error.h"
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

constexpr base::StringPiece kTestEndpoint = "http://example.com";

template <typename Response>
class Receiver {
 public:
  const base::expected<std::unique_ptr<Response>, ProtoFetcherStatus>&
  GetResult() const {
    return result_;
  }

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
  base::expected<std::unique_ptr<Response>, ProtoFetcherStatus> result_;
};

// Go around Gtest limitation for test parametrization - create aliases for type
// tuples
using ClassifyUrl = std::tuple<ClassifyUrlRequest, ClassifyUrlResponse>;
using ListFamilyMembers =
    std::tuple<ListFamilyMembersRequest, ListFamilyMembersResponse>;

// Tests the kidsmanagement/v1 proto client.
template <typename T>
class ProtoFetcherTest : public testing::Test {
 protected:
  // Go around Gtest limitation for test parametrization - extract Request and
  // Response.
  using Request = typename std::tuple_element<0, T>::type;
  using Response = typename std::tuple_element<1, T>::type;

  FetcherConfig test_fetcher_config_ =
      FetcherTestConfigBuilder::FromConfig(GetConfig())
          .WithServiceEndpoint(kTestEndpoint)
          .Build();
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
  IdentityTestEnvironment identity_test_env_;

  // Both GetConfig and CreateFetcher methods are required because respective
  // config and fetchers are not generic on purpose. The network connection is
  // mocked and requests are only validate server side, so it doesn't make sense
  // to send anything but default Proto.
  FetcherConfig GetConfig() const;

  // Creates the fetcher. The returned unique_ptr will either hold a started
  // fetcher (::ProtoFetcher), or will be empty for a started deferred fetcher
  // (::DeferredProtoFetcher).
  std::unique_ptr<ProtoFetcher<Response>> CreateFetcher(
      Receiver<Response>& receiver,
      const Request& request = Request());
};

template <>
FetcherConfig ProtoFetcherTest<ListFamilyMembers>::GetConfig() const {
  return kListFamilyMembersConfig;
}
template <>
FetcherConfig ProtoFetcherTest<ClassifyUrl>::GetConfig() const {
  return kClassifyUrlConfig;
}

template <>
std::unique_ptr<ProtoFetcher<ListFamilyMembersResponse>>
ProtoFetcherTest<ListFamilyMembers>::CreateFetcher(
    Receiver<ListFamilyMembersResponse>& receiver,
    const Request& request) {
  return FetchListFamilyMembers(
      *identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      BindOnce(&Receiver<ListFamilyMembersResponse>::Receive,
               base::Unretained(&receiver)),
      test_fetcher_config_);  // Unretained(.) must outlive the fetcher.
}

template <>
std::unique_ptr<ProtoFetcher<ClassifyUrlResponse>>
ProtoFetcherTest<ClassifyUrl>::CreateFetcher(
    Receiver<ClassifyUrlResponse>& receiver,
    const Request& request) {
  std::unique_ptr<DeferredProtoFetcher<ClassifyUrlResponse>> fetcher =
      ClassifyURL(
          *identity_test_env_.identity_manager(),
          test_url_loader_factory_.GetSafeWeakWrapper(), request,
          test_fetcher_config_);  // Unretained(.) must outlive the fetcher.

  auto* fetcher_ptr = fetcher.get();
  fetcher_ptr->Start(BindOnce(&Receiver<ClassifyUrlResponse>::ReceiveDeferred,
                              base::Unretained(&receiver), std::move(fetcher)));

  // Empty unique_ptr for DeferredProtoFetcher.
  return std::unique_ptr<ProtoFetcher<ClassifyUrlResponse>>();
}

TYPED_TEST_SUITE_P(ProtoFetcherTest);

TYPED_TEST_P(ProtoFetcherTest, ConfiguresEndpoint) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);

  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("access_token",
                                                               Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);

  GURL expected_url =
      GURL("http://example.com/" + std::string(this->GetConfig().service_path) +
           "?alt=proto");
  EXPECT_EQ(pending_request->request.url, expected_url);
  EXPECT_EQ(pending_request->request.method, this->GetConfig().GetHttpMethod());
}

TYPED_TEST_P(ProtoFetcherTest, AddsPayload) {
  if (this->GetConfig().method != FetcherConfig::Method::kPost) {
    GTEST_SKIP() << "Payload not supported for "
                 << this->GetConfig().GetHttpMethod() << " requests.";
  }

  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);

  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("access_token",
                                                               Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);

  std::string header;
  EXPECT_TRUE(pending_request->request.headers.GetHeader(
      net::HttpRequestHeaders::kContentType, &header));
  EXPECT_EQ(header, "application/x-protobuf");
}

TYPED_TEST_P(ProtoFetcherTest, AcceptsRequests) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;
  Response response;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);
  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("access_token",
                                                               Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);

  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response.SerializeAsString());

  EXPECT_TRUE(receiver.GetResult().has_value());
}

TYPED_TEST_P(ProtoFetcherTest, NoAccessToken) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);
  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(
              GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(this->test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(receiver.GetResult().error().state(),
            ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR);
  EXPECT_EQ(receiver.GetResult().error().google_service_auth_error().state(),
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
}

TYPED_TEST_P(ProtoFetcherTest, HandlesMalformedResponse) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);
  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("access_token",
                                                               Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);

  std::string malformed_value("garbage");  // Not a valid marshaled proto.
  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), malformed_value);
  EXPECT_FALSE(receiver.GetResult().has_value());
  EXPECT_EQ(receiver.GetResult().error().state(),
            ProtoFetcherStatus::State::INVALID_RESPONSE);
}

// crbug/1444165: Do not use StringPrintf with StringPiece, c-strings are
// expected.
TYPED_TEST_P(ProtoFetcherTest, CreatesToken) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);
  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("token",
                                                               Time::Max());

  // That's enough: request is pending, so token is accepted.
  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);

  // Only check header format here.
  std::string authorization_header;
  ASSERT_TRUE(pending_request->request.headers.GetHeader(
      net::HttpRequestHeaders::kAuthorization, &authorization_header));
  EXPECT_EQ(authorization_header, "Bearer token");
}

TYPED_TEST_P(ProtoFetcherTest, HandlesNetworkError) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);
  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("access_token",
                                                               Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);

  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url,
      network::URLLoaderCompletionStatus(net::ERR_UNEXPECTED),
      network::CreateURLResponseHead(net::HTTP_OK), /*content=*/"");
  EXPECT_FALSE(receiver.GetResult().has_value());
  EXPECT_EQ(receiver.GetResult().error().state(),
            ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  EXPECT_EQ(receiver.GetResult().error().http_status_or_net_error(),
            ProtoFetcherStatus::HttpStatusOrNetErrorType(net::ERR_UNEXPECTED));
}

TYPED_TEST_P(ProtoFetcherTest, HandlesServerError) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);
  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("access_token",
                                                               Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);

  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), /*content=*/"",
      net::HTTP_BAD_REQUEST);
  EXPECT_FALSE(receiver.GetResult().has_value());
  EXPECT_EQ(receiver.GetResult().error().state(),
            ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  EXPECT_EQ(
      receiver.GetResult().error().http_status_or_net_error(),
      ProtoFetcherStatus::HttpStatusOrNetErrorType(net::HTTP_BAD_REQUEST));
}

TYPED_TEST_P(ProtoFetcherTest, RecordsMetrics) {
  using Response = typename std::tuple_element<1, TypeParam>::type;
  Receiver<Response> receiver;
  Response response;
  base::HistogramTester histogram_tester;

  AccountInfo account = this->identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);

  auto fetcher = this->CreateFetcher(receiver);
  this->identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("access_token",
                                                               Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      this->test_url_loader_factory_.GetPendingRequest(0);

  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response.SerializeAsString());

  ASSERT_TRUE(receiver.GetResult().has_value());

  // The actual latency of mocked fetch is variable, so only expect that some
  // value was recorded.
  histogram_tester.ExpectTotalCount(
      base::StrCat({this->GetConfig().histogram_basename, ".Latency"}),
      /*expected_count(grew by)*/ 1);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          base::StrCat({this->GetConfig().histogram_basename, ".Status"})),
      base::BucketsInclude(base::Bucket(ProtoFetcherStatus::State::OK, 1)));

  // Tests that no enum above ::OK was emitted:
  EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                  {this->GetConfig().histogram_basename, ".Status"})),
              base::BucketsInclude(base::Bucket(
                  ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR, 0)));
}

REGISTER_TYPED_TEST_SUITE_P(ProtoFetcherTest,
                            ConfiguresEndpoint,
                            AddsPayload,
                            AcceptsRequests,
                            NoAccessToken,
                            HandlesMalformedResponse,
                            CreatesToken,
                            HandlesNetworkError,
                            HandlesServerError,
                            RecordsMetrics);

using Fetchers = ::testing::Types<ClassifyUrl, ListFamilyMembers>;
INSTANTIATE_TYPED_TEST_SUITE_P(AllFetchers, ProtoFetcherTest, Fetchers);

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
    return supervised_user::ClassifyURL(*identity_manager,
                                        url_loader_factory.GetSafeWeakWrapper(),
                                        request, config);
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
  ASSERT_EQ(this->test_url_loader_factory_.NumPending(), 2L);
  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      this->test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      this->test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
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
    ASSERT_EQ(this->test_url_loader_factory_.NumPending(), 2L);

    // Now under_test will go out of scope.
  }

  // Unblocking network traffic won't help executing callbacks, since their
  // parent manager |under_test| is now gone.
  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      this->test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      this->test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
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
  ASSERT_EQ(this->test_url_loader_factory_.NumPending(), 1L);
  this->test_url_loader_factory_.SimulateResponseForPendingRequest(
      this->test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      CreatePermissionRequestResponse().SerializeAsString());
}

}  // namespace
}  // namespace supervised_user
