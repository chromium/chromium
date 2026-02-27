// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_proxy.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/testing/fake_connection.h"
#include "components/private_ai/testing/fake_token_manager.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

using ::testing::_;
using ::testing::Invoke;

class ConnectionProxyTest : public testing::Test {
 public:
  ConnectionProxyTest() {
    // Initialize the real NetworkService for testing.
    // This runs on the current thread and doesn't require Mojo pipe binding.
    network_service_ = network::NetworkService::CreateForTesting();
  }
  ~ConnectionProxyTest() override = default;

  void CreateConnectionProxy() {
    // Pass the raw pointer of the real service.
    // network::NetworkService inherits from network::mojom::NetworkService.
    connection_proxy_ = std::make_unique<ConnectionProxy>(
        GURL("https://proxy.example.com"), &token_manager_,
        network_service_.get(),
        base::BindOnce(&ConnectionProxyTest::CreateInnerConnection,
                       base::Unretained(this)),
        base::BindOnce(&ConnectionProxyTest::OnDisconnect,
                       base::Unretained(this)));
  }

  std::unique_ptr<Connection> CreateInnerConnection(
      network::mojom::NetworkContext* context) {
    auto connection = std::make_unique<FakeConnection>(base::DoNothing());
    inner_connection_ = connection.get();
    if (on_inner_connection_created_) {
      std::move(on_inner_connection_created_).Run();
    }
    return connection;
  }

  void OnDisconnect(ErrorCode error_code) {
    on_disconnect_called_ = true;
    connection_proxy_->OnDestroy(error_code);
  }

 protected:
  // Use MainThreadType::IO to satisfy NetworkService threading
  // requirements.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  FakeTokenManager token_manager_;
  std::unique_ptr<network::NetworkService> network_service_;
  std::unique_ptr<ConnectionProxy> connection_proxy_;
  raw_ptr<FakeConnection> inner_connection_ = nullptr;
  base::OnceClosure on_inner_connection_created_;

  bool on_disconnect_called_ = false;
};

TEST_F(ConnectionProxyTest, Success) {
  CreateConnectionProxy();

  // Send a request. It should be buffered.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  proto::PrivateAiRequest request;
  request.set_request_id(1);
  connection_proxy_->Send(std::move(request), base::Seconds(10),
                          future.GetCallback());

  EXPECT_FALSE(inner_connection_);

  // Provide the token.
  base::test::TestFuture<void> inner_connection_created_future;
  on_inner_connection_created_ = inner_connection_created_future.GetCallback();
  token_manager_.RespondToGetAuthTokenForProxy(phosphor::BlindSignedAuthToken{
      .token = FakeTokenManager::kFakeProxyToken});
  EXPECT_TRUE(inner_connection_created_future.Wait());

  // Inner connection should be created and request forwarded.
  ASSERT_TRUE(inner_connection_);
  ASSERT_EQ(inner_connection_->pending_requests().size(), 1u);
  EXPECT_EQ(inner_connection_->pending_requests()[0].request.request_id(), 1);

  // Respond to request.
  proto::PrivateAiResponse response;
  response.set_request_id(1);
  std::move(inner_connection_->pending_requests()[0].callback)
      .Run(std::move(response));

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConnectionProxyTest, SendAfterInitialization) {
  CreateConnectionProxy();

  // Provide the token to complete initialization.
  base::test::TestFuture<void> inner_connection_created_future;
  on_inner_connection_created_ = inner_connection_created_future.GetCallback();
  token_manager_.RespondToGetAuthTokenForProxy(phosphor::BlindSignedAuthToken{
      .token = FakeTokenManager::kFakeProxyToken});
  EXPECT_TRUE(inner_connection_created_future.Wait());
  ASSERT_TRUE(inner_connection_);

  // Send a request. It should be sent directly.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  proto::PrivateAiRequest request;
  request.set_request_id(1);
  connection_proxy_->Send(std::move(request), base::Seconds(10),
                          future.GetCallback());

  ASSERT_EQ(inner_connection_->pending_requests().size(), 1u);
  EXPECT_EQ(inner_connection_->pending_requests()[0].request.request_id(), 1);

  // Respond to request.
  proto::PrivateAiResponse response;
  response.set_request_id(1);
  std::move(inner_connection_->pending_requests()[0].callback)
      .Run(std::move(response));

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConnectionProxyTest, FailsWithEmptyProxyUrl) {
  EXPECT_CHECK_DEATH((void)std::make_unique<ConnectionProxy>(
      GURL(), &token_manager_, network_service_.get(),
      base::BindOnce(&ConnectionProxyTest::CreateInnerConnection,
                     base::Unretained(this)),
      base::BindOnce(&ConnectionProxyTest::OnDisconnect,
                     base::Unretained(this))));
}

TEST_F(ConnectionProxyTest, ProxyTokenFailure) {
  CreateConnectionProxy();

  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;
  connection_proxy_->Send(proto::PrivateAiRequest(), base::Seconds(10),
                          future.GetCallback());

  // Fail token fetch.
  token_manager_.RespondToGetAuthTokenForProxy(std::nullopt);

  EXPECT_FALSE(inner_connection_);
  EXPECT_TRUE(on_disconnect_called_);

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kError);

  // A subsequent request should also fail.
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future2;
  connection_proxy_->Send(proto::PrivateAiRequest(), base::Seconds(10),
                          future2.GetCallback());
  auto result2 = future2.Get();
  EXPECT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), ErrorCode::kError);
}

}  // namespace

}  // namespace private_ai
