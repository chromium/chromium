// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/network/personal_context_fetcher.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_test_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

const char kTestEndpointUrl[] =
    "https://contextmemoryservice-pa.googleapis.com/v1:fetchContext";
const char kTestPiiEndpointUrl[] =
    "https://contextmemoryservice-pa.googleapis.com/v1:fetchPiiEntities";

class PersonalContextFetcherTest : public testing::Test {
 public:
  PersonalContextFetcherTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPersonalContext);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    fetcher_ = std::make_unique<PersonalContextFetcher>(
        proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
        identity_test_env_.identity_manager(), shared_url_loader_factory_,
        GURL("https://contextmemoryservice-pa.googleapis.com/v1"));
  }

  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    variations::VariationsIdsProvider::GetInstance()
        ->ForceVariationIdsForTesting({"12", "34"}, "");
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<PersonalContextFetcher> fetcher_;
};

TEST_F(PersonalContextFetcherTest, FetchAppendsVariationsHeader) {
  base::test::TestMessage request_metadata;
  base::test::TestFuture<
      base::expected<const proto::FetchContextResponse, ContextMemoryError>>
      future;
  fetcher_->FetchContext(request_metadata, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestEndpointUrl);

  // Verify the variations X-Client-Data header is appended and resolves the
  // forced IDs.
  std::optional<std::string> variations_header =
      pending_request->request.cors_exempt_headers.GetHeader(
          variations::kClientDataHeader);
  ASSERT_TRUE(variations_header.has_value());

  std::set<variations::VariationID> variation_ids;
  std::set<variations::VariationID> trigger_ids;
  ASSERT_TRUE(variations::ExtractVariationIds(*variations_header,
                                              &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.contains(12));
  EXPECT_TRUE(variation_ids.contains(34));
}

TEST_F(PersonalContextFetcherTest, FetchSuccess) {
  base::HistogramTester histogram_tester;
  base::test::TestMessage request_metadata;
  request_metadata.set_test("test_metadata_value");

  base::test::TestFuture<
      base::expected<const proto::FetchContextResponse, ContextMemoryError>>
      future;
  fetcher_->FetchContext(request_metadata, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestEndpointUrl);
  EXPECT_EQ(pending_request->request.method, "POST");

  // Verify request body upload data
  std::string upload_data = network::GetUploadData(pending_request->request);
  proto::FetchContextRequest request_body;
  ASSERT_TRUE(request_body.ParseFromString(upload_data));
  EXPECT_EQ(request_body.feature(),
            proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  EXPECT_EQ(request_body.request_metadata().type_url(),
            "type.googleapis.com/base.test.TestMessage");

  base::test::TestMessage parsed_metadata;
  ASSERT_TRUE(
      parsed_metadata.ParseFromString(request_body.request_metadata().value()));
  EXPECT_EQ(parsed_metadata.test(), "test_metadata_value");

  proto::FetchContextResponse fetch_response;
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestEndpointUrl, fetch_response.SerializeAsString());

  auto response = future.Get();
  ASSERT_TRUE(response.has_value());

  histogram_tester.ExpectTotalCount(
      "PersonalContext.FetchContext.ErrorStatus.AmbientAutofill", 0);
}

TEST_F(PersonalContextFetcherTest, FetchWithTimeout) {
  base::test::TestMessage request_metadata;
  base::test::TestFuture<
      base::expected<const proto::FetchContextResponse, ContextMemoryError>>
      future;
  fetcher_->FetchContext(request_metadata, base::Seconds(30),
                         future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestEndpointUrl);

  std::optional<std::string> timeout_header =
      pending_request->request.headers.GetHeader("X-Server-Timeout");
  ASSERT_TRUE(timeout_header.has_value());
  EXPECT_EQ(timeout_header.value(), "30");

  proto::FetchContextResponse fetch_response;
  fetch_response.set_server_request_id("test_timeout_request_id");
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestEndpointUrl, fetch_response.SerializeAsString());

  auto response = future.Get();
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->server_request_id(), "test_timeout_request_id");
}

TEST_F(PersonalContextFetcherTest, FetchFailure) {
  base::HistogramTester histogram_tester;
  base::test::TestMessage request_metadata;
  base::test::TestFuture<
      base::expected<const proto::FetchContextResponse, ContextMemoryError>>
      future;
  fetcher_->FetchContext(request_metadata, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestEndpointUrl);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_INTERNAL_SERVER_ERROR);

  auto response = future.Get();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().error(),
            ContextMemoryError::ExecutionError::kGenericFailure);

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchContext.ErrorStatus.AmbientAutofill",
      static_cast<int>(ContextMemoryError::ExecutionError::kGenericFailure), 1);
}

TEST_F(PersonalContextFetcherTest, DestructWhileFetchInProgress) {
  base::HistogramTester histogram_tester;
  base::test::TestMessage request_metadata;
  base::test::TestFuture<
      base::expected<const proto::FetchContextResponse, ContextMemoryError>>
      future;
  fetcher_->FetchContext(request_metadata, std::nullopt, future.GetCallback());

  // Destruct the fetcher.
  fetcher_.reset();

  auto response = future.Get();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().error(),
            ContextMemoryError::ExecutionError::kCancelled);

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchContext.ErrorStatus.AmbientAutofill",
      static_cast<int>(ContextMemoryError::ExecutionError::kCancelled), 1);
}

TEST_F(PersonalContextFetcherTest, FetchInvalidResponseBody) {
  base::HistogramTester histogram_tester;
  base::test::TestMessage request_metadata;
  base::test::TestFuture<
      base::expected<const proto::FetchContextResponse, ContextMemoryError>>
      future;
  fetcher_->FetchContext(request_metadata, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);

  // Simulate an invalid response data
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "invalid_payload_bytes");

  auto response = future.Get();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().error(),
            ContextMemoryError::ExecutionError::kGenericFailure);

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchContext.ErrorStatus.AmbientAutofill",
      static_cast<int>(ContextMemoryError::ExecutionError::kGenericFailure), 1);
}

TEST_F(PersonalContextFetcherTest, FetchPiiSuccess) {
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future;
  proto::FetchPiiEntitiesRequest pii_request;
  fetcher_->FetchPiiEntities(pii_request, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestPiiEndpointUrl);
  EXPECT_EQ(pending_request->request.method, "POST");

  proto::FetchPiiEntitiesResponse fetch_response;
  fetch_response.add_entities();
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestPiiEndpointUrl, fetch_response.SerializeAsString());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value().entities_size(), 1);
}

TEST_F(PersonalContextFetcherTest, FetchPiiServerError) {
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future;
  proto::FetchPiiEntitiesRequest pii_request;
  fetcher_->FetchPiiEntities(pii_request, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestPiiEndpointUrl, "", net::HTTP_INTERNAL_SERVER_ERROR);

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().error(),
            ContextMemoryError::ExecutionError::kGenericFailure);
}

TEST_F(PersonalContextFetcherTest, FetchPiiDestructionCancellation) {
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future;
  proto::FetchPiiEntitiesRequest pii_request;
  fetcher_->FetchPiiEntities(pii_request, std::nullopt, future.GetCallback());

  fetcher_.reset();
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().error(),
            ContextMemoryError::ExecutionError::kCancelled);
}

TEST_F(PersonalContextFetcherTest, FetchPiiWithCustomTimeout) {
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future;
  proto::FetchPiiEntitiesRequest pii_request;
  fetcher_->FetchPiiEntities(pii_request, base::Seconds(45),
                             future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestPiiEndpointUrl);

  std::optional<std::string> timeout_header =
      pending_request->request.headers.GetHeader("X-Server-Timeout");
  ASSERT_TRUE(timeout_header.has_value());
  EXPECT_EQ(timeout_header.value(), "45");

  proto::FetchPiiEntitiesResponse fetch_response;
  fetch_response.add_entities();
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestPiiEndpointUrl, fetch_response.SerializeAsString());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value().entities_size(), 1);
}

TEST_F(PersonalContextFetcherTest, FetchPiiOAuthTokenFailure) {
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future;
  proto::FetchPiiEntitiesRequest pii_request;
  fetcher_->FetchPiiEntities(pii_request, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().error(),
            ContextMemoryError::ExecutionError::kPermissionDenied);
}

TEST_F(PersonalContextFetcherTest, FetchPiiConcurrentRequestRejection) {
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future1;
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future2;
  proto::FetchPiiEntitiesRequest pii_request1;
  proto::FetchPiiEntitiesRequest pii_request2;

  fetcher_->FetchPiiEntities(pii_request1, std::nullopt, future1.GetCallback());

  fetcher_->FetchPiiEntities(pii_request2, std::nullopt, future2.GetCallback());

  ASSERT_FALSE(future2.Get().has_value());
  EXPECT_EQ(future2.Get().error().error(),
            ContextMemoryError::ExecutionError::kGenericFailure);

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  proto::FetchPiiEntitiesResponse fetch_response;
  fetch_response.add_entities();
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestPiiEndpointUrl, fetch_response.SerializeAsString());

  ASSERT_TRUE(future1.Get().has_value());
  EXPECT_EQ(future1.Get().value().entities_size(), 1);
}

}  // namespace
}  // namespace personal_context
