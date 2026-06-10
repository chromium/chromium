// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_service_impl.h"

#include <string>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {

namespace {

using ::base::test::TestMessage;

proto::FetchContextResponse BuildFetchContextResponse(std::string_view output) {
  proto::FetchContextResponse fetch_response;
  proto::Any* any_metadata = fetch_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/test.Message");
  any_metadata->set_value(std::string(output));
  return fetch_response;
}

class PersonalContextServiceImplTest : public testing::Test {
 public:
  PersonalContextServiceImplTest() = default;
  ~PersonalContextServiceImplTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPersonalContext,
        {{features::kContextMemoryServiceBaseUrl.name,
          "https://example.com/v1"}});
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    personal_context_service_ = std::make_unique<PersonalContextServiceImpl>(
        url_loader_factory_, identity_test_env_.identity_manager());
  }

  void SetAutomaticIssueOfAccessTokens() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://example.com/v1:fetchContext", content, http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  bool SimulateSuccessfulResponse() {
    std::string serialized_response;
    proto::FetchContextResponse fetch_response =
        BuildFetchContextResponse("foo response");
    fetch_response.SerializeToString(&serialized_response);
    return SimulateResponse(serialized_response, net::HTTP_OK);
  }

  PersonalContextServiceImpl* personal_context_service() {
    return personal_context_service_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<PersonalContextServiceImpl> personal_context_service_;
};

TEST_F(PersonalContextServiceImplTest, FetchContextDelegatesToManager) {
  SetAutomaticIssueOfAccessTokens();

  base::test::TestFuture<FetchContextResult> future;

  ContextMemoryRequestOptions options;
  personal_context_service()->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, TestMessage(), options,
      future.GetCallback());

  EXPECT_TRUE(SimulateSuccessfulResponse());

  FetchContextResult result = future.Take();
  ASSERT_TRUE(result.response.has_value());
  ASSERT_EQ("foo response", result.response.value().value());
}

TEST_F(PersonalContextServiceImplTest, FetchPiiEntitiesDelegatesToManager) {
  SetAutomaticIssueOfAccessTokens();

  base::test::TestFuture<FetchPiiEntitiesResult> future;

  proto::FetchPiiEntitiesRequest request;
  request.set_feature(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  ContextMemoryRequestOptions options;
  personal_context_service()->FetchPiiEntities(request, options,
                                               future.GetCallback());

  proto::FetchPiiEntitiesResponse pii_response;
  pii_response.set_server_request_id("test_id");
  std::string serialized_response;
  pii_response.SerializeToString(&serialized_response);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.com/v1:fetchPiiEntities", serialized_response,
      net::HTTP_OK, network::TestURLLoaderFactory::kUrlMatchPrefix);

  FetchPiiEntitiesResult result = future.Take();
  ASSERT_TRUE(result.response.has_value());
  EXPECT_EQ("test_id", result.response.value().server_request_id());
}

}  // namespace

}  // namespace personal_context
