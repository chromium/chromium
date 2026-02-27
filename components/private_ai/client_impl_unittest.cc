// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/client_impl.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/testing/fake_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

class FakeConnectionFactory : public ConnectionFactory {
 public:
  FakeConnectionFactory() = default;
  ~FakeConnectionFactory() override = default;

  std::unique_ptr<Connection> Create(
      base::OnceCallback<void(ErrorCode)> on_disconnect) override {
    auto connection = std::make_unique<FakeConnection>(
        std::move(on_disconnect), std::move(on_destruction_));
    last_connection_ = connection.get();
    return connection;
  }

  FakeConnection* last_connection() {
    CHECK(last_connection_);
    return last_connection_;
  }

  void set_on_destruction(base::OnceClosure on_destruction) {
    on_destruction_ = std::move(on_destruction);
  }

  void SimulateDisconnectAndDestroyConnection() {
    CHECK(last_connection_);

    // Use a raw pointer to avoid dangling pointer checks.
    // We must reset `last_connection_` before calling `SimulateDisconnect()`
    // because it will trigger the destruction of the FakeConnection.
    FakeConnection* fake_connection = last_connection_;
    last_connection_ = nullptr;
    fake_connection->SimulateDisconnect();
  }

 private:
  raw_ptr<FakeConnection> last_connection_;
  base::OnceClosure on_destruction_;
};

void ResolvePendingRequest(
    FakeConnection* connection,
    base::expected<proto::PrivateAiResponse, ErrorCode> result) {
  CHECK(connection);
  CHECK(!connection->pending_requests().empty());
  auto pending_request = std::move(connection->pending_requests().front());
  connection->pending_requests().erase(connection->pending_requests().begin());
  std::move(pending_request.callback).Run(std::move(result));
}

}  // namespace

class ClientImplTest : public ::testing::Test {
 public:
  ClientImplTest() {
    auto factory = std::make_unique<FakeConnectionFactory>();
    factory_ = factory.get();
    client_ = std::make_unique<ClientImpl>(std::move(factory),
                                           std::make_unique<PrivateAiLogger>());
  }

  ~ClientImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<Client> client_;
  raw_ptr<FakeConnectionFactory> factory_;
};

// Test the successful request flow.
TEST_F(ClientImplTest, SendTextRequestSuccess) {
  const std::string kExpectedResponseText = "response text";

  proto::PrivateAiResponse private_ai_response;
  {
    auto* generate_content_response =
        private_ai_response.mutable_generate_content_response();
    auto* candidate = generate_content_response->add_candidates();
    auto* content = candidate->mutable_content();
    content->set_role("model");
    auto* part = content->add_parts();
    part->set_text(kExpectedResponseText);
  }

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback(), /*options=*/{});

  ResolvePendingRequest(factory_->last_connection(),
                        base::ok(private_ai_response));

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), kExpectedResponseText);
}

// Test the successful request flow for paic requests.
TEST_F(ClientImplTest, SendPaicRequestSuccess) {
  proto::PrivateAiResponse private_ai_response;
  private_ai_response.mutable_paic_response();

  base::test::TestFuture<base::expected<proto::PaicMessage, ErrorCode>> future;
  client_->SendPaicRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           proto::PaicMessage(), future.GetCallback(),
                           /*options=*/{});

  ResolvePendingRequest(factory_->last_connection(),
                        base::ok(private_ai_response));

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
}

// Test that the connection is recreated after a disconnect.
TEST_F(ClientImplTest, ConnectionRecreation) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback(), /*options=*/{});

  // Simulate disconnect.
  auto* first_connection = factory_->last_connection();
  ASSERT_TRUE(first_connection);
  factory_->SimulateDisconnectAndDestroyConnection();

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNetworkError);

  // A subsequent request should succeed on the new connection.
  const std::string kExpectedResponseText = "response text";
  proto::PrivateAiResponse private_ai_response;
  private_ai_response.mutable_generate_content_response()
      ->add_candidates()
      ->mutable_content()
      ->add_parts()
      ->set_text(kExpectedResponseText);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> second_future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some other text", second_future.GetCallback(),
                           /*options=*/{});

  auto* second_connection = factory_->last_connection();
  ASSERT_NE(first_connection, second_connection);

  ResolvePendingRequest(second_connection, base::ok(private_ai_response));

  const auto& second_result = second_future.Get();
  ASSERT_TRUE(second_result.has_value());
  EXPECT_EQ(second_result.value(), kExpectedResponseText);
}

// Test that a request times out correctly.
TEST_F(ClientImplTest, SendTextRequestTimeout) {
  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback(),
                           /*options=*/{.timeout = base::Seconds(10)});

  ResolvePendingRequest(factory_->last_connection(),
                        base::unexpected(ErrorCode::kTimeout));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kTimeout);
}

// Test that an empty response from the server is handled correctly.
TEST_F(ClientImplTest, SendTextRequestEmptyResponse) {
  proto::PrivateAiResponse private_ai_response;
  private_ai_response.mutable_generate_content_response();

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback(), /*options=*/{});

  ResolvePendingRequest(factory_->last_connection(),
                        base::ok(private_ai_response));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNoContent);
}

// Test that a malformed response from the server is handled correctly.
TEST_F(ClientImplTest, SendGenerateContentRequestMalformedResponse) {
  base::test::TestFuture<
      base::expected<proto::GenerateContentResponse, ErrorCode>>
      future;
  client_->SendGenerateContentRequest(
      proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
      proto::GenerateContentRequest(), future.GetCallback(), /*options=*/{});

  // Response missing GenerateContentResponse.
  ResolvePendingRequest(factory_->last_connection(),
                        base::ok(proto::PrivateAiResponse()));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNoResponse);
}

// Test that the connection is destroyed asynchronously after a disconnect.
TEST_F(ClientImplTest, AsyncDisconnect) {
  base::test::TestFuture<void> destroyed_future;
  factory_->set_on_destruction(destroyed_future.GetCallback());

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client_->SendTextRequest(proto::FeatureName::FEATURE_NAME_UNSPECIFIED,
                           "some text", future.GetCallback(), /*options=*/{});

  auto* connection = factory_->last_connection();
  ASSERT_TRUE(connection);

  // Simulate disconnect.
  factory_->SimulateDisconnectAndDestroyConnection();

  // The connection should not be destroyed immediately.
  EXPECT_FALSE(destroyed_future.IsReady());

  // Running pending tasks should destroy the connection.
  EXPECT_TRUE(destroyed_future.Wait());
}

}  // namespace private_ai
