// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_bootstrap {

class PendingConnectionManagerTest : public testing::Test {
 protected:
  PendingConnectionManager connection_manager_;
};

namespace {

TEST_F(PendingConnectionManagerTest, Basic) {
  auto token = base::UnguessableToken::Create();
  int callback_calls = 0;
  connection_manager_.ExpectOpenIpcChannel(
      token,
      base::BindLambdaForTesting([&](base::ScopedFD fd) { callback_calls++; }));
  EXPECT_TRUE(connection_manager_.OpenIpcChannel(token.ToString(), {}));
  EXPECT_FALSE(connection_manager_.OpenIpcChannel(token.ToString(), {}));
  EXPECT_EQ(1, callback_calls);
}

TEST_F(PendingConnectionManagerTest, Cancel) {
  auto token = base::UnguessableToken::Create();
  connection_manager_.ExpectOpenIpcChannel(
      token, base::BindLambdaForTesting(
                 [&](base::ScopedFD fd) { FAIL() << "Unexpected call"; }));
  connection_manager_.CancelExpectedOpenIpcChannel(token);
  EXPECT_FALSE(connection_manager_.OpenIpcChannel(token.ToString(), {}));
}

TEST_F(PendingConnectionManagerTest, UnexpectedConnection) {
  EXPECT_FALSE(connection_manager_.OpenIpcChannel("invalid", {}));
}

}  // namespace
}  // namespace mojo_bootstrap
