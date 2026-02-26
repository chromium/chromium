// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/private_ai/client_impl.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection_basic.h"
#include "components/private_ai/connection_metrics.h"
#include "components/private_ai/connection_timeout.h"
#include "components/private_ai/connection_token_attestation.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/secure_channel.h"
#include "components/private_ai/testing/fake_secure_channel.h"
#include "components/private_ai/testing/fake_token_manager.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

using ::testing::_;
using ::testing::Invoke;

class IntegrationConnectionFactory : public ConnectionFactory {
 public:
  IntegrationConnectionFactory(
      FakeSecureChannelFactory::OnCreatedCallback on_secure_channel_created,
      FakeSecureChannelFactory::OnDestroyedCallback on_secure_channel_destroyed,
      phosphor::TokenManager* token_manager,
      PrivateAiLogger* logger)
      : on_secure_channel_created_(std::move(on_secure_channel_created)),
        on_secure_channel_destroyed_(std::move(on_secure_channel_destroyed)),
        token_manager_(token_manager),
        logger_(logger) {}

  std::unique_ptr<Connection> Create(
      base::OnceCallback<void(ErrorCode)> on_disconnect) override {
    auto split_on_disconnect =
        base::SplitOnceCallback(std::move(on_disconnect));

    auto secure_channel_factory = std::make_unique<FakeSecureChannelFactory>(
        on_secure_channel_created_, on_secure_channel_destroyed_);

    std::unique_ptr<Connection> connection =
        std::make_unique<ConnectionBasic>(std::move(secure_channel_factory),
                                          std::move(split_on_disconnect.first));

    connection = std::make_unique<ConnectionMetrics>(std::move(connection));

    connection = std::make_unique<ConnectionTimeout>(std::move(connection));

    connection = std::make_unique<ConnectionTokenAttestation>(
        std::move(connection), token_manager_, logger_,
        std::move(split_on_disconnect.second));

    return connection;
  }

 private:
  FakeSecureChannelFactory::OnCreatedCallback on_secure_channel_created_;
  FakeSecureChannelFactory::OnDestroyedCallback on_secure_channel_destroyed_;
  raw_ptr<phosphor::TokenManager> token_manager_;
  raw_ptr<PrivateAiLogger> logger_;
};

}  // namespace

class ClientImplIntegrationTest : public testing::Test {
 public:
  void SetUp() override {
    auto logger = std::make_unique<PrivateAiLogger>();
    PrivateAiLogger* logger_ptr = logger.get();
    auto factory = std::make_unique<IntegrationConnectionFactory>(
        base::BindRepeating(
            &ClientImplIntegrationTest::on_secure_channel_created,
            base::Unretained(this)),
        base::BindRepeating(
            &ClientImplIntegrationTest::on_secure_channel_destroyed,
            base::Unretained(this)),
        &token_manager_, logger_ptr);

    client_ =
        std::make_unique<ClientImpl>(std::move(factory), std::move(logger));
  }

  void TearDown() override {
    // Ensure that all SecureChannels are destroyed.
    client_.reset();
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return secure_channels_.empty(); }));
  }

  void on_secure_channel_created(FakeSecureChannel* secure_channel) {
    secure_channels_.push_back(secure_channel);
  }

  void on_secure_channel_destroyed(FakeSecureChannel* secure_channel) {
    std::erase(secure_channels_, secure_channel);
  }

  FakeSecureChannel* last_secure_channel() {
    if (secure_channels_.empty()) {
      return nullptr;
    }
    return secure_channels_.back();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FakeTokenManager token_manager_;
  std::vector<raw_ptr<FakeSecureChannel>> secure_channels_;
  std::unique_ptr<ClientImpl> client_;
};

TEST_F(ClientImplIntegrationTest, FullStackSuccess) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "hello", future.GetCallback(), /*options=*/{});

  // 1. Attestation starts. FakeTokenManager gets a request.
  EXPECT_EQ(token_manager_.GetPendingCallbackCount(), 1u);
  token_manager_.RunPendingCallbacks();

  // 2. SecureChannel (Basic) is created and gets the attestation request.
  auto* channel = last_secure_channel();
  ASSERT_TRUE(channel);
  EXPECT_TRUE(channel->last_written_request().has_anonymous_token_request());

  // 3. Respond to attestation.
  proto::PrivateAiResponse attestation_response;
  attestation_response.set_request_id(
      channel->last_written_request().request_id());
  channel->send_back_response(attestation_response);

  // 4. Now the original text request should be sent.
  EXPECT_TRUE(channel->last_written_request().has_generate_content_request());

  // 5. Respond to text request.
  proto::PrivateAiResponse text_response;
  text_response.set_request_id(channel->last_written_request().request_id());
  text_response.mutable_generate_content_response()
      ->add_candidates()
      ->mutable_content()
      ->add_parts()
      ->set_text("world");
  channel->send_back_response(text_response);

  // 6. Verify final result.
  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "world");
}

TEST_F(ClientImplIntegrationTest, AttestationFailure) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "hello", future.GetCallback(), /*options=*/{});

  // 1. Attestation starts.
  EXPECT_EQ(token_manager_.GetPendingCallbackCount(), 1u);
  // Simulate token fetch failure.
  token_manager_.RespondToGetAuthToken(std::nullopt);

  // 2. Client should receive an error.
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kClientAttestationFailed);
}

TEST_F(ClientImplIntegrationTest, Timeout) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "hello", future.GetCallback(),
                           {.timeout = base::Seconds(5)});

  // Complete attestation successfully.
  token_manager_.RunPendingCallbacks();
  auto* channel = last_secure_channel();
  ASSERT_TRUE(channel);
  proto::PrivateAiResponse attestation_response;
  attestation_response.set_request_id(
      channel->last_written_request().request_id());
  channel->send_back_response(attestation_response);

  // Text request is sent.
  EXPECT_TRUE(channel->last_written_request().has_generate_content_request());

  // Wait for timeout.
  task_environment_.FastForwardBy(base::Seconds(6));

  // Verify timeout error.
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kTimeout);
}

TEST_F(ClientImplIntegrationTest, ConcurrentRequestsDuringAttestation) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future1;
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future2;

  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "request1", future1.GetCallback(), /*options=*/{});
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "request2", future2.GetCallback(), /*options=*/{});

  // 1. Attestation starts (only one token fetch should be triggered).
  EXPECT_EQ(token_manager_.GetPendingCallbackCount(), 1u);
  token_manager_.RunPendingCallbacks();

  auto* channel = last_secure_channel();
  ASSERT_TRUE(channel);

  // 2. Complete attestation.
  proto::PrivateAiResponse attestation_response;
  attestation_response.set_request_id(
      channel->last_written_request().request_id());
  channel->send_back_response(attestation_response);

  // 3. Now both requests should be sent.
  ASSERT_EQ(channel->written_requests().size(), 3u);

  // Handle request 1
  EXPECT_EQ(channel->written_requests()[1]
                .generate_content_request()
                .contents(0)
                .parts(0)
                .text(),
            "request1");
  int32_t id1 = channel->written_requests()[1].request_id();
  proto::PrivateAiResponse resp1;
  resp1.set_request_id(id1);
  resp1.mutable_generate_content_response()
      ->add_candidates()
      ->mutable_content()
      ->add_parts()
      ->set_text("response1");
  channel->send_back_response(resp1);

  // Handle request 2
  EXPECT_EQ(channel->written_requests()[2]
                .generate_content_request()
                .contents(0)
                .parts(0)
                .text(),
            "request2");
  int32_t id2 = channel->written_requests()[2].request_id();
  proto::PrivateAiResponse resp2;
  resp2.set_request_id(id2);
  resp2.mutable_generate_content_response()
      ->add_candidates()
      ->mutable_content()
      ->add_parts()
      ->set_text("response2");
  channel->send_back_response(resp2);

  // 4. Verify both results.
  EXPECT_EQ(future1.Get().value(), "response1");
  EXPECT_EQ(future2.Get().value(), "response2");
}

TEST_F(ClientImplIntegrationTest, DisconnectDuringAttestation) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "hello", future.GetCallback(), /*options=*/{});

  // 1. Attestation starts.
  token_manager_.RunPendingCallbacks();
  auto* channel = last_secure_channel();
  ASSERT_TRUE(channel);

  // 2. Simulate channel disconnect before responding to attestation.
  channel->send_back_error(ErrorCode::kNetworkError);

  // 3. The original request should fail with the disconnect error.
  ASSERT_TRUE(future.IsReady());
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNetworkError);
}

TEST_F(ClientImplIntegrationTest, ClientDestroyedDuringAttestation) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "hello", future.GetCallback(), /*options=*/{});

  // 1. Attestation starts.
  token_manager_.RunPendingCallbacks();

  // 2. Destroy the client while attestation is pending.
  client_.reset();

  // 3. The request should be resolved with kDestroyed.
  ASSERT_TRUE(future.IsReady());
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kDestroyed);
}

TEST_F(ClientImplIntegrationTest, AttestationTimedOut) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "hello", future.GetCallback(),
                           {.timeout = base::Seconds(5)});

  // 1. Attestation starts.
  token_manager_.RunPendingCallbacks();
  auto* channel = last_secure_channel();
  ASSERT_TRUE(channel);

  // 2. Wait for the attestation timeout (60 seconds).
  task_environment_.FastForwardBy(base::Seconds(61));

  // 3. Result should be kClientAttestationFailed because
  // ConnectionTokenAttestation converts any error during attestation response
  // to kClientAttestationFailed.
  ASSERT_TRUE(future.IsReady());
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kClientAttestationFailed);
}

}  // namespace private_ai
