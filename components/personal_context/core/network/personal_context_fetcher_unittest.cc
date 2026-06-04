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
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

const char kTestBaseUrl[] = "https://example.com/v1";
const char kTestEndpointUrl[] = "https://example.com/v1:fetchContext";

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

}  // namespace
}  // namespace personal_context
