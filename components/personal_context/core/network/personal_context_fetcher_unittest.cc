// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/network/personal_context_fetcher.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

const char kTestBaseUrl[] = "https://example.com/v1";
const char kTestEndpointUrl[] = "https://example.com/v1:fetchContext";
const char kTestPiiEndpointUrl[] = "https://example.com/v1:fetchPiiEntities";

class PersonalContextFetcherTest : public testing::Test {
 public:
  PersonalContextFetcherTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPersonalContext,
        {{features::kContextMemoryServiceBaseUrl.name, kTestBaseUrl}});

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    fetcher_ = std::make_unique<PersonalContextFetcher>(
        identity_test_env_.identity_manager(), shared_url_loader_factory_);
  }

  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<PersonalContextFetcher> fetcher_;
};

TEST_F(PersonalContextFetcherTest, FetchSuccess) {
  base::RunLoop run_loop;
  base::test::TestMessage request_metadata;
  fetcher_->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, request_metadata,
      std::nullopt,
      base::BindLambdaForTesting(
          [&](base::expected<const proto::FetchContextResponse,
                             ContextMemoryError> response) {
            ASSERT_TRUE(response.has_value());
            run_loop.Quit();
          }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestEndpointUrl);
  EXPECT_EQ(pending_request->request.method, "POST");

  proto::FetchContextResponse fetch_response;
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestEndpointUrl, fetch_response.SerializeAsString());

  run_loop.Run();
}

TEST_F(PersonalContextFetcherTest, FetchWithTimeout) {
  base::RunLoop run_loop;
  base::test::TestMessage request_metadata;
  fetcher_->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, request_metadata,
      base::Seconds(30),
      base::BindLambdaForTesting(
          [&](base::expected<const proto::FetchContextResponse,
                             ContextMemoryError> response) {
            ASSERT_TRUE(response.has_value());
            run_loop.Quit();
          }));

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
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestEndpointUrl, fetch_response.SerializeAsString());

  run_loop.Run();
}

TEST_F(PersonalContextFetcherTest, FetchPiiSuccess) {
  base::test::TestFuture<
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>>
      future;
  proto::FetchPiiEntitiesRequest pii_request;
  fetcher_->FetchPiiEntities(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                             pii_request, std::nullopt, future.GetCallback());

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
  fetcher_->FetchPiiEntities(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                             pii_request, std::nullopt, future.GetCallback());

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
  fetcher_->FetchPiiEntities(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                             pii_request, std::nullopt, future.GetCallback());

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
  fetcher_->FetchPiiEntities(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                             pii_request, base::Seconds(45),
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
  fetcher_->FetchPiiEntities(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                             pii_request, std::nullopt, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

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

  fetcher_->FetchPiiEntities(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                             pii_request1, std::nullopt, future1.GetCallback());

  fetcher_->FetchPiiEntities(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                             pii_request2, std::nullopt, future2.GetCallback());

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
