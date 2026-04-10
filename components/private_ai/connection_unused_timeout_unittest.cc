// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_unused_timeout.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/status_code.h"
#include "components/private_ai/testing/fake_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

constexpr base::TimeDelta kUnusedTimeout = base::Seconds(30);

class ConnectionUnusedTimeoutTest : public testing::Test {
 public:
  ConnectionUnusedTimeoutTest() {
    auto fake_connection = std::make_unique<FakeConnection>(base::DoNothing());
    fake_connection_ = fake_connection.get();
    connection_unused_timeout_ = std::make_unique<ConnectionUnusedTimeout>(
        std::move(fake_connection),
        base::BindOnce(&ConnectionUnusedTimeoutTest::OnDisconnect,
                       base::Unretained(this)),
        kUnusedTimeout);
  }

  ~ConnectionUnusedTimeoutTest() override = default;

  void OnDisconnect(StatusCode status_code) {
    disconnect_error_ = status_code;
    is_disconnected_ = true;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<ConnectionUnusedTimeout> connection_unused_timeout_;
  raw_ptr<FakeConnection> fake_connection_;

  bool is_disconnected_ = false;
  std::optional<StatusCode> disconnect_error_;
};

TEST_F(ConnectionUnusedTimeoutTest, TimeoutFires) {
  // Advance time to just before the timeout.
  task_environment_.FastForwardBy(kUnusedTimeout - base::Milliseconds(1));
  EXPECT_FALSE(is_disconnected_);

  // Advance time to trigger unused timeout.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(is_disconnected_);
  EXPECT_EQ(disconnect_error_, StatusCode::kUnusedConnection);
}

TEST_F(ConnectionUnusedTimeoutTest, SendResetsTimeout) {
  // Advance time to just before the timeout.
  task_environment_.FastForwardBy(kUnusedTimeout - base::Milliseconds(1));
  EXPECT_FALSE(is_disconnected_);

  // Sending a request resets unused timeout.
  connection_unused_timeout_->Send(proto::PrivateAiRequest(), base::Seconds(10),
                                   base::DoNothing());

  // Advance time by kUnusedTimeout - 1ms. Timer should not fire.
  task_environment_.FastForwardBy(kUnusedTimeout - base::Milliseconds(1));
  EXPECT_FALSE(is_disconnected_);

  // Advance time by 1ms to trigger the reset timeout.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(is_disconnected_);
  EXPECT_EQ(disconnect_error_, StatusCode::kUnusedConnection);
}

TEST_F(ConnectionUnusedTimeoutTest, OnDestroyStopsTimeout) {
  connection_unused_timeout_->OnDestroy(StatusCode::kDestroyed);

  // Advance time. The Timeout should have been stopped, so no disconnect
  // callback from unused timeout.
  task_environment_.FastForwardBy(kUnusedTimeout + base::Seconds(1));
  EXPECT_FALSE(is_disconnected_);
}

}  // namespace

}  // namespace private_ai
