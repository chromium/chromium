// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_service.h"

#include "base/test/task_environment.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/variations_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_search {

namespace {

using testing::_;
using testing::IsNull;
using testing::NotNull;

class FakeVariationsClient : public variations::VariationsClient {
 public:
  FakeVariationsClient() = default;
  ~FakeVariationsClient() override = default;

  // variations::VariationsClient:
  bool IsOffTheRecord() const override { return false; }
  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::mojom::VariationsHeaders::New();
  }
};

}  // namespace

class ContextualSearchServiceTest : public testing::Test {
 public:
  ContextualSearchServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    service_ = std::make_unique<ContextualSearchService>(
        identity_test_env_.identity_manager(), test_shared_loader_factory_,
        search_engines_test_environment_.template_url_service(),
        &fake_variations_client_, version_info::Channel::UNKNOWN, "en-US");
  }

  lens::ClientToAimMessage CaptureClientToAimRequest(
      std::unique_ptr<
          ContextualSearchContextController::CreateClientToAimRequestInfo>
          create_client_to_aim_request_info) {
    captured_client_to_aim_message_ =
        std::move(create_client_to_aim_request_info);
    return lens::ClientToAimMessage();
  }

  ContextualSearchContextController::CreateClientToAimRequestInfo*
  GetCapturedClientToAimRequest() {
    return captured_client_to_aim_message_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  FakeVariationsClient fake_variations_client_;
  std::unique_ptr<ContextualSearchService> service_;
  std::unique_ptr<
      ContextualSearchContextController::CreateClientToAimRequestInfo>
      captured_client_to_aim_message_;
};

TEST_F(ContextualSearchServiceTest, Session) {
  // Try to get a session that does not exist.
  auto bad_handle = service_->GetSession(base::UnguessableToken::Create());
  ASSERT_THAT(bad_handle, IsNull());

  // Create a new session.
  auto config_params1 =
      std::make_unique<ContextualSearchContextController::ConfigParams>();
  config_params1->send_lns_surface = false;
  config_params1->suppress_lns_surface_param_if_no_image = true;
  config_params1->enable_multi_context_input_flow = false;
  config_params1->enable_viewport_images = false;
  auto session1_handle1 = service_->CreateSession(
      std::move(config_params1), ContextualSearchSource::kUnknown);
  ASSERT_THAT(session1_handle1, NotNull());
  ASSERT_THAT(session1_handle1->GetController(), NotNull());
  ASSERT_THAT(session1_handle1->GetMetricsRecorder(), NotNull());

  // Create another new session.
  auto config_params2 =
      std::make_unique<ContextualSearchContextController::ConfigParams>();
  config_params2->send_lns_surface = false;
  config_params2->suppress_lns_surface_param_if_no_image = true;
  config_params2->enable_multi_context_input_flow = false;
  config_params2->enable_viewport_images = false;
  auto session2_handle1 = service_->CreateSession(
      std::move(config_params2), ContextualSearchSource::kUnknown);
  ASSERT_THAT(session2_handle1, NotNull());
  ASSERT_THAT(session2_handle1->GetController(), NotNull());
  ASSERT_THAT(session2_handle1->GetMetricsRecorder(), NotNull());

  // Get a new handle to session two.
  auto session2_handle2 = service_->GetSession(session2_handle1->session_id());
  ASSERT_THAT(session2_handle2, NotNull());
  EXPECT_EQ(session2_handle2->GetController(),
            session2_handle1->GetController());
  EXPECT_EQ(session2_handle2->GetMetricsRecorder(),
            session2_handle1->GetMetricsRecorder());
  EXPECT_NE(session1_handle1->GetMetricsRecorder(),
            session2_handle1->GetMetricsRecorder());
  EXPECT_NE(session1_handle1->GetController(),
            session2_handle1->GetController());

  // Release the first handle to session two. The session should still be alive.
  session2_handle1.reset();
  auto session2_handle3 = service_->GetSession(session2_handle2->session_id());
  ASSERT_THAT(session2_handle3, NotNull());
  EXPECT_EQ(session2_handle3->GetController(),
            session2_handle2->GetController());
  EXPECT_EQ(session2_handle3->GetMetricsRecorder(),
            session2_handle2->GetMetricsRecorder());

  // Release the remaining handles to session two. The session should be
  // released.
  auto session_id = session2_handle2->session_id();
  session2_handle2.reset();
  session2_handle3.reset();
  auto session2_handle4 = service_->GetSession(session_id);
  ASSERT_THAT(session2_handle4, IsNull());

  // Get a new handle to session one.
  auto session1_handle2 = service_->GetSession(session1_handle1->session_id());
  ASSERT_THAT(session1_handle2, NotNull());
  EXPECT_EQ(session1_handle2->GetController(),
            session1_handle1->GetController());
  EXPECT_EQ(session1_handle2->GetMetricsRecorder(),
            session1_handle1->GetMetricsRecorder());
}

TEST_F(ContextualSearchServiceTest, PendingContextTokens) {
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto metrics_recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      ContextualSearchSource::kUnknown);

  MockContextualSearchContextController* mock_controller_ptr =
      mock_controller.get();

  auto session_handle = service_->CreateSessionForTesting(
      std::move(mock_controller), std::move(metrics_recorder));

  // Add some dummy tokens.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  session_handle->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle->GetUploadedContextTokensForTesting().push_back(token2);

  // Capture the tokens before they are moved.
  std::vector<base::UnguessableToken> expected_request_tokens1;
  expected_request_tokens1.push_back(token1);
  expected_request_tokens1.push_back(token2);

  // Expect CreateClientToAimRequest to be called and capture the message.
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Invoke(
          this, &ContextualSearchServiceTest::CaptureClientToAimRequest));

  // Call CreateClientToAimRequest to move tokens to pending.
  auto create_client_to_aim_request_info = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  session_handle->CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info));

  // Verify that the request contains the correct unguessable tokens.
  EXPECT_THAT(GetCapturedClientToAimRequest()->file_tokens,
              testing::UnorderedElementsAreArray(expected_request_tokens1));

  // Verify that submitted context tokens contains the moved tokens.
  std::vector<base::UnguessableToken> expected_submitted_tokens;
  expected_submitted_tokens.push_back(token1);
  expected_submitted_tokens.push_back(token2);
  EXPECT_THAT(session_handle->GetSubmittedContextTokens(),
              testing::UnorderedElementsAreArray(expected_submitted_tokens));
  EXPECT_TRUE(session_handle->GetUploadedContextTokens().empty());

  // Add more tokens and call CreateClientToAimRequest again.
  base::UnguessableToken token3 = base::UnguessableToken::Create();
  session_handle->GetUploadedContextTokensForTesting().push_back(token3);

  // Capture the tokens for the second request.
  std::vector<base::UnguessableToken> expected_request_tokens2;
  expected_request_tokens2.push_back(token3);

  // Expect CreateClientToAimRequest to be called again and capture the message.
  EXPECT_CALL(*mock_controller_ptr, CreateClientToAimRequest(_))
      .WillOnce(testing::Invoke(
          this, &ContextualSearchServiceTest::CaptureClientToAimRequest));

  auto create_client_to_aim_request_info2 = std::make_unique<
      ContextualSearchContextController::CreateClientToAimRequestInfo>();
  session_handle->CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info2));

  // Verify that the second request contains only the newly added token.
  EXPECT_THAT(GetCapturedClientToAimRequest()->file_tokens,
              testing::UnorderedElementsAreArray(expected_request_tokens2));

  // Verify that the submitted context tokens has both the previous and the new
  // tokens.
  expected_submitted_tokens.push_back(token3);
  EXPECT_THAT(session_handle->GetSubmittedContextTokens(),
              testing::UnorderedElementsAreArray(expected_submitted_tokens));
  EXPECT_TRUE(session_handle->GetUploadedContextTokens().empty());

  // Clear the pending tokens.
  session_handle->ClearSubmittedContextTokens();

  // Verify that the submitted context tokens are empty.
  EXPECT_TRUE(session_handle->GetSubmittedContextTokens().empty());
}

}  // namespace contextual_search
