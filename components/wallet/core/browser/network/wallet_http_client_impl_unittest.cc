// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/wallet_http_client_impl.h"

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/metrics/wallet_metrics.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "components/wallet/core/browser/proto/pass.pb.h"
#include "components/wallet/core/common/wallet_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

namespace wallet {
namespace {

using testing::Eq;
using testing::Optional;

constexpr char kAccessToken[] = "test access token";
constexpr base::TimeDelta kLatency = base::Milliseconds(250);

using UpsertPublicPassCallback = base::test::TestFuture<
    const base::expected<std::string, WalletHttpClient::WalletRequestError>&>;

class WalletHttpClientImplTest : public testing::Test {
 public:
  WalletHttpClientImplTest() = default;

  ~WalletHttpClientImplTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kWalletApiPrivatePassesEnabled,
          {{"wallet_pass_save_url", "https://test-wallet.com/"}}},
         {features::kWalletablePassDetection, {}}},
        {});
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    client_ = std::make_unique<WalletHttpClientImpl>(
        identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  void TearDown() override { client_.reset(); }

  GURL GetUpsertPassUrl() {
    return GURL(features::kWalletSaveUrl.Get()).Resolve("v1/passes:upsert");
  }

  GURL GetUpsertPrivatePassUrl() {
    return GURL(features::kWalletSaveUrl.Get())
        .Resolve("v1/e/privatePasses:upsert");
  }

  GURL GetUnmaskedPassUrl() {
    return GURL(features::kWalletSaveUrl.Get())
        .Resolve("v1/e/privatePasses:batchGet");
  }

  WalletHttpClientImpl* client() { return client_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<WalletHttpClientImpl> client_;
};

// Tests that the http client sets the proper Content-Type header.
TEST_F(WalletHttpClientImplTest, ContentType) {
  Pass pass;
  UpsertPublicPassCallback upsert_pass_callback;
  client()->UpsertPublicPass(pass, upsert_pass_callback.GetCallback());

  // Access token is fetched successfully.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory()->GetPendingRequest(0);
  ASSERT_TRUE(pending_request);

  std::optional<std::string> content_type =
      pending_request->request.headers.GetHeader(
          net::HttpRequestHeaders::kContentType);
  EXPECT_THAT(content_type, Optional(Eq("application/protobuf")));
}

// Tests that UpsertPublicPass successfully triggers a network request and
// invokes the callback with a success result when the server responds with
// success.
TEST_F(WalletHttpClientImplTest, UpsertPublicPass_Success) {
  base::HistogramTester histogram_tester;
  Pass pass;
  UpsertPublicPassCallback upsert_pass_callback;
  client()->UpsertPublicPass(pass, upsert_pass_callback.GetCallback());

  // Access token is fetched successfully.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  // Handle the network response.
  GURL expected_url = GetUpsertPassUrl();
  EXPECT_TRUE(test_url_loader_factory()->IsPending(expected_url.spec()));
  std::optional<std::string> auth_header =
      test_url_loader_factory()
          ->GetPendingRequest(0)
          ->request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization);
  EXPECT_EQ(auth_header.value_or(std::string()),
            base::StrCat({"Bearer ", kAccessToken}));

  api::UpsertPassResponse response;
  response.set_pass_id("pass-id");
  test_url_loader_factory()->AddResponse(expected_url.spec(),
                                         response.SerializeAsString());

  ASSERT_TRUE(upsert_pass_callback.Wait());
  EXPECT_TRUE(upsert_pass_callback.Get().has_value());
  EXPECT_EQ(upsert_pass_callback.Get().value(), "pass-id");
  histogram_tester.ExpectUniqueSample("Wallet.NetworkRequest.OauthError",
                                      GoogleServiceAuthError::NONE, 1);
}

// Tests that UpsertPublicPass correctly handles server errors by invoking the
// callback with a failure result.
TEST_F(WalletHttpClientImplTest, UpsertPublicPass_TokenFetchError) {
  base::HistogramTester histogram_tester;
  Pass pass;
  Pass_LoyaltyCard* loyalty_card = pass.mutable_loyalty_card();
  loyalty_card->set_program_name("Program Name");
  loyalty_card->set_merchant_name("Issuer Name");
  loyalty_card->set_loyalty_number("Member ID");

  UpsertPublicPassCallback upsert_pass_callback;
  client()->UpsertPublicPass(pass, upsert_pass_callback.GetCallback());

  // Access token fetch fails.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

  ASSERT_TRUE(upsert_pass_callback.Wait());
  EXPECT_EQ(upsert_pass_callback.Get().error(),
            WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed);
  histogram_tester.ExpectUniqueSample("Wallet.NetworkRequest.OauthError",
                                      GoogleServiceAuthError::CONNECTION_FAILED,
                                      1);
}

TEST_F(WalletHttpClientImplTest, UpsertPublicPass_Failure) {
  Pass pass;
  UpsertPublicPassCallback upsert_pass_callback;
  client()->UpsertPublicPass(pass, upsert_pass_callback.GetCallback());

  // Access token is fetched successfully.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  GURL expected_url = GetUpsertPassUrl();
  EXPECT_TRUE(test_url_loader_factory()->IsPending(expected_url.spec()));
  test_url_loader_factory()->AddResponse(
      expected_url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  ASSERT_TRUE(upsert_pass_callback.Wait());
  EXPECT_EQ(upsert_pass_callback.Get().error(),
            WalletHttpClient::WalletRequestError::kGenericError);
}

TEST_F(WalletHttpClientImplTest, UpsertPublicPass_NetErrorCode) {
  base::HistogramTester histogram_tester;
  Pass pass;
  UpsertPublicPassCallback upsert_pass_callback;
  client()->UpsertPublicPass(pass, upsert_pass_callback.GetCallback());

  // Access token is fetched successfully.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  GURL expected_url = GetUpsertPassUrl();
  EXPECT_TRUE(test_url_loader_factory()->IsPending(expected_url.spec()));
  test_url_loader_factory()->AddResponse(
      expected_url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT));

  ASSERT_TRUE(upsert_pass_callback.Wait());
  ASSERT_FALSE(upsert_pass_callback.Get().has_value());
  EXPECT_EQ(upsert_pass_callback.Get().error(),
            WalletHttpClient::WalletRequestError::kGenericError);
  histogram_tester.ExpectUniqueSample(
      "Wallet.NetworkRequest.UpsertPass.HttpResponseOrErrorCode",
      net::ERR_TIMED_OUT, 1);
}

TEST_F(WalletHttpClientImplTest, UpsertPublicPass_HttpError) {
  base::HistogramTester histogram_tester;
  Pass pass;
  UpsertPublicPassCallback upsert_pass_callback;
  client()->UpsertPublicPass(pass, upsert_pass_callback.GetCallback());

  // Access token is fetched successfully.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  GURL expected_url = GetUpsertPassUrl();
  EXPECT_TRUE(test_url_loader_factory()->IsPending(expected_url.spec()));
  test_url_loader_factory()->AddResponse(expected_url.spec(), "",
                                         net::HTTP_INTERNAL_SERVER_ERROR);

  ASSERT_TRUE(upsert_pass_callback.Wait());
  ASSERT_FALSE(upsert_pass_callback.Get().has_value());
  EXPECT_EQ(upsert_pass_callback.Get().error(),
            WalletHttpClient::WalletRequestError::kGenericError);
  histogram_tester.ExpectUniqueSample(
      "Wallet.NetworkRequest.UpsertPass.HttpResponseOrErrorCode",
      net::HTTP_INTERNAL_SERVER_ERROR, 1);
}

// Tests that multiple UpsertPublicPass requests can be in-flight simultaneously
// and all callbacks are invoked correctly upon completion.
TEST_F(WalletHttpClientImplTest, UpsertPublicPass_ConcurrentRequests) {
  Pass pass1;
  pass1.mutable_loyalty_card()->set_program_name("p1");

  Pass pass2;
  pass2.mutable_loyalty_card()->set_program_name("p2");

  UpsertPublicPassCallback upsert_pass_callback1;
  UpsertPublicPassCallback upsert_pass_callback2;

  client()->UpsertPublicPass(pass1, upsert_pass_callback1.GetCallback());
  client()->UpsertPublicPass(pass2, upsert_pass_callback2.GetCallback());

  // Access token is fetched successfully.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  ASSERT_EQ(test_url_loader_factory()->NumPending(), 2);

  // Complete both requests.
  GURL expected_url = GetUpsertPassUrl();
  api::UpsertPassResponse response;
  response.set_pass_id("pass-id");
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      expected_url.spec(), response.SerializeAsString());
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      expected_url.spec(), response.SerializeAsString());

  ASSERT_TRUE(upsert_pass_callback1.Wait());
  ASSERT_TRUE(upsert_pass_callback2.Wait());
}

// Tests that UpsertPublicPass for a LoyaltyCard builds the correct proto
// request body structure.
TEST_F(WalletHttpClientImplTest,
       UpsertPublicPass_LoyaltyCard_RequestStructure) {
  Pass pass;
  auto* loyalty_card = pass.mutable_loyalty_card();
  loyalty_card->set_program_name("p1");
  loyalty_card->set_merchant_name("i1");
  loyalty_card->set_loyalty_number("m1");

  UpsertPublicPassCallback upsert_pass_callback;
  client()->UpsertPublicPass(pass, upsert_pass_callback.GetCallback());

  // Access token is fetched successfully.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  GURL expected_url = GetUpsertPassUrl();
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory()->GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.url, expected_url);

  std::string request_body = network::GetUploadData(pending_request->request);
  api::UpsertPassRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));

  // Verify pass
  ASSERT_TRUE(request_proto.has_pass());
  const Pass& pass_proto = request_proto.pass();

  // Verify loyalty_card
  ASSERT_TRUE(pass_proto.has_loyalty_card());
  EXPECT_EQ(pass_proto.loyalty_card().merchant_name(), "i1");
  EXPECT_EQ(pass_proto.loyalty_card().loyalty_number(), "m1");
  EXPECT_EQ(pass_proto.loyalty_card().program_name(), "p1");

  // Verify client_info
  ASSERT_TRUE(request_proto.has_client_info());
  EXPECT_EQ(request_proto.client_info().chrome_client_info().version(),
            version_info::GetVersionNumber());
}

TEST_F(WalletHttpClientImplTest, UpsertPrivatePass_RequestHeaders) {
  PrivatePass pass;
  pass.mutable_passport();
  base::test::TestFuture<
      const base::expected<PrivatePass, WalletHttpClient::WalletRequestError>&>
      callback;
  client()->UpsertPrivatePass(pass, callback.GetCallback());

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  GURL expected_url = GetUpsertPrivatePassUrl();
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory()->GetPendingRequest(0);
  ASSERT_TRUE(pending_request);

  EXPECT_EQ(pending_request->request.headers.GetHeader("EES-S7E-Mode"),
            "proto");
  EXPECT_EQ(
      pending_request->request.headers.GetHeader("EES-Proto-Tokenization"),
      "1.3.2;574");
}

TEST_F(WalletHttpClientImplTest, UpsertPass_Latency) {
  base::HistogramTester histogram_tester;
  UpsertPublicPassCallback callback;
  client()->UpsertPublicPass(Pass(), callback.GetCallback());
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  task_environment().FastForwardBy(kLatency);
  api::UpsertPassResponse response;
  response.set_pass_id("pass-id");
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      GetUpsertPassUrl().spec(), response.SerializeAsString());

  histogram_tester.ExpectUniqueTimeSample(
      "Wallet.NetworkRequest.UpsertPass.Latency", kLatency, 1);
}

TEST_F(WalletHttpClientImplTest, UpsertPass_ResponseSize) {
  base::HistogramTester histogram_tester;
  UpsertPublicPassCallback callback;
  client()->UpsertPublicPass(Pass(), callback.GetCallback());
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  api::UpsertPassResponse response;
  response.set_pass_id("pass-id");
  std::string response_string = response.SerializeAsString();
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      GetUpsertPassUrl().spec(), response_string);

  histogram_tester.ExpectUniqueSample(
      "Wallet.NetworkRequest.UpsertPass.ResponseByteSize",
      response_string.size(), 1);
}

TEST_F(WalletHttpClientImplTest, UpsertPrivatePass_Latency) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      const base::expected<PrivatePass, WalletHttpClient::WalletRequestError>&>
      callback;
  PrivatePass pass;
  pass.mutable_passport();
  client()->UpsertPrivatePass(pass, callback.GetCallback());
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  task_environment().FastForwardBy(kLatency);
  api::UpsertPrivatePassResponse response;
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      GetUpsertPrivatePassUrl().spec(), response.SerializeAsString());

  histogram_tester.ExpectUniqueTimeSample(
      "Wallet.NetworkRequest.UpsertPrivatePass.Latency", kLatency, 1);
}

TEST_F(WalletHttpClientImplTest, GetUnmaskedPass_Latency) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      const base::expected<PrivatePass, WalletHttpClient::WalletRequestError>&>
      callback;
  client()->GetUnmaskedPass("pass-id", callback.GetCallback());
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  task_environment().FastForwardBy(kLatency);
  api::GetPrivatePassesResponse response;
  response.add_results();
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      GetUnmaskedPassUrl().spec(), response.SerializeAsString());

  histogram_tester.ExpectUniqueTimeSample(
      "Wallet.NetworkRequest.GetUnmaskedPrivatePass.Latency", kLatency, 1);
}

}  // namespace
}  // namespace wallet
