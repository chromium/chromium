// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_timeout.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/testing/fake_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

class ConnectionTimeoutTest : public testing::Test {
 public:
  ConnectionTimeoutTest() {
    auto fake_connection = std::make_unique<FakeConnection>(base::DoNothing());
    fake_connection_ = fake_connection.get();
    connection_timeout_ =
        std::make_unique<ConnectionTimeout>(std::move(fake_connection));
  }

  ~ConnectionTimeoutTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<ConnectionTimeout> connection_timeout_;
  raw_ptr<FakeConnection> fake_connection_;
};

TEST_F(ConnectionTimeoutTest, Success) {
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;

  connection_timeout_->Send(proto::PrivateAiRequest(), base::Seconds(10),
                            future.GetCallback());

  ASSERT_EQ(fake_connection_->pending_requests().size(), 1u);

  std::move(fake_connection_->pending_requests()[0].callback)
      .Run(proto::PrivateAiResponse());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConnectionTimeoutTest, Timeout) {
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future;

  proto::PrivateAiRequest request;
  connection_timeout_->Send(std::move(request), base::Seconds(10),
                            future.GetCallback());

  ASSERT_EQ(fake_connection_->pending_requests().size(), 1u);

  // Advance time to trigger timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kTimeout);

  // Ensure that late response is ignored.
  std::move(fake_connection_->pending_requests()[0].callback)
      .Run(proto::PrivateAiResponse());
}

// Tests that multiple requests are handled correctly, including timeout for one
// request and successful response for another one.
TEST_F(ConnectionTimeoutTest, MultipleRequests) {
  base::test::TestFuture<base::expected<proto::PrivateAiResponse, ErrorCode>>
      future1, future2;

  connection_timeout_->Send(proto::PrivateAiRequest(), base::Seconds(10),
                            future1.GetCallback());
  connection_timeout_->Send(proto::PrivateAiRequest(), base::Seconds(20),
                            future2.GetCallback());

  ASSERT_EQ(fake_connection_->pending_requests().size(), 2u);

  // Trigger timeout for the first request.
  task_environment_.FastForwardBy(base::Seconds(10));

  auto result1 = future1.Get();
  ASSERT_FALSE(result1.has_value());
  EXPECT_EQ(result1.error(), ErrorCode::kTimeout);

  // Second request should still be pending.
  ASSERT_FALSE(future2.IsReady());

  // Resolve second request successfully.
  proto::PrivateAiResponse response2;
  response2.set_request_id(2);
  std::move(fake_connection_->pending_requests()[1].callback)
      .Run(std::move(response2));

  auto result2 = future2.Get();
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2.value().request_id(), 2);
}

}  // namespace

}  // namespace private_ai
