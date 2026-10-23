// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/ios/private_ai_oak_session_driver_ios.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace private_ai {

class PrivateAiOakSessionDriverIOSTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
  PrivateAiOakSessionDriverIOS driver_;
};

TEST_F(PrivateAiOakSessionDriverIOSTest, SingleSessionLifecycle) {
  // 1. Initially no sessions.
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 0u);

  // 2. Bind a session.
  mojo::Remote<mojom::OakSession> remote = driver_.BindOakSessionService();
  EXPECT_TRUE(remote.is_bound());
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 1u);

  // 3. Destroy the remote.
  remote.reset();
  EXPECT_FALSE(remote.is_bound());

  // 4. The cleanup happens asynchronously on the task runner. Run until idle.
  base::RunLoop().RunUntilIdle();

  // 5. Verify the session was cleaned up.
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 0u);
}

TEST_F(PrivateAiOakSessionDriverIOSTest, MultipleConcurrentSessions) {
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 0u);

  // 1. Bind two sessions.
  mojo::Remote<mojom::OakSession> remote1 = driver_.BindOakSessionService();
  mojo::Remote<mojom::OakSession> remote2 = driver_.BindOakSessionService();

  EXPECT_TRUE(remote1.is_bound());
  EXPECT_TRUE(remote2.is_bound());
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 2u);

  // 2. Destroy the first remote.
  remote1.reset();
  base::RunLoop().RunUntilIdle();

  // 3. Verify only the first session is cleaned up, and the second remains.
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 1u);
  EXPECT_TRUE(remote2.is_bound());

  // 4. Verify we can still interact with the second remote (it is not
  // disconnected).
  bool callback_called = false;
  remote2->InitiateHandshake(base::BindOnce(
      [](bool* called, HandshakeMessage message) { *called = true; },
      &callback_called));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);

  // 5. Destroy the second remote.
  remote2.reset();
  base::RunLoop().RunUntilIdle();

  // 6. Verify all sessions are cleaned up.
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 0u);
}

TEST_F(PrivateAiOakSessionDriverIOSTest, SingleTlsSessionLifecycle) {
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 0u);

  mojo::Remote<mojom::OakTlsSession> remote =
      driver_.BindOakTlsSessionService();
  EXPECT_TRUE(remote.is_bound());
  EXPECT_EQ(driver_.GetActiveSessionsCountForTesting(), 1u);

  remote.reset();
  EXPECT_FALSE(remote.is_bound());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return driver_.GetActiveSessionsCountForTesting() == 0u; }));
}

}  // namespace private_ai
