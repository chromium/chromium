// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace legion {

using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using base::test::RunOnceCallback;

namespace {

// Mock implementation of the SecureChannel interface.
class MockSecureChannelClient : public SecureChannel {
 public:
  MockSecureChannelClient() = default;
  ~MockSecureChannelClient() override = default;

  MOCK_METHOD(
      void,
      Write,
      (Request request, SecureChannel::OnResponseReceivedCallback callback),
      (override));
};

}  // namespace

class ClientTest : public ::testing::Test {
 public:
  ClientTest() = default;
  ~ClientTest() override = default;

  using RequestFuture =
      base::test::TestFuture<ResultCode, std::optional<Response>>;

  void SetUp() override {
    auto mock_secure_channel = std::make_unique<MockSecureChannelClient>();
    mock_secure_channel_ = mock_secure_channel.get();
    client_ = std::make_unique<Client>(
        std::move(mock_secure_channel), "test_api_key");
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<Client> client_;
  raw_ptr<MockSecureChannelClient> mock_secure_channel_;  // Owned by client_
};

// Test the successful request flow.
TEST_F(ClientTest, SendRequestSuccess) {
  Request request = {1, 2, 3};
  Response expected_response_data = {4, 5, 6};

  EXPECT_CALL(*mock_secure_channel_, Write(Eq(request), _))
      .WillOnce(
          RunOnceCallback<1>(ResultCode::kSuccess, expected_response_data));

  RequestFuture future;
  client_->SendRequest(request, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kSuccess);
  ASSERT_TRUE(future.Get<1>().has_value());
  EXPECT_EQ(*future.Get<1>(), expected_response_data);
}

// Test the flow where the SecureChannel indicates a network error.
TEST_F(ClientTest, SendRequestNetworkError) {
  Request request = {7, 8, 9};

  EXPECT_CALL(*mock_secure_channel_, Write(Eq(request), _))
      .WillOnce(RunOnceCallback<1>(ResultCode::kNetworkError, std::nullopt));

  RequestFuture future;
  client_->SendRequest(request, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kNetworkError);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Test the flow where the SecureChannel indicates a generic error.
TEST_F(ClientTest, SendRequestGenericError) {
  Request request = {10, 11, 12};

  EXPECT_CALL(*mock_secure_channel_, Write(Eq(request), _))
      .WillOnce(RunOnceCallback<1>(ResultCode::kError, std::nullopt));

  RequestFuture future;
  client_->SendRequest(request, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kError);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Test the flow where authentication fails due to an empty API key.
TEST_F(ClientTest, SendRequestAuthenticationFailed) {
  auto mock_secure_channel = std::make_unique<MockSecureChannelClient>();
  EXPECT_CALL(*mock_secure_channel, Write(_, _)).Times(0);

  // Create a client with an empty API key.
  auto client = std::make_unique<Client>(
      std::move(mock_secure_channel), "");

  Request request = {13, 14, 15};

  RequestFuture future;
  client->SendRequest(request, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kAuthenticationFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

}  // namespace legion
