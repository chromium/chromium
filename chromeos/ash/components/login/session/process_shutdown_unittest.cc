// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/session/process_shutdown.h"

#include "chromeos/ash/components/login/session/scoped_process_shutting_down_override_for_testing.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(ProcessShutdownTest, FollowsKeepAliveRegistry) {
  KeepAliveRegistry& registry = *KeepAliveRegistry::GetInstance();
  ASSERT_FALSE(registry.IsShuttingDown());
  EXPECT_FALSE(IsProcessShuttingDown());

  registry.SetIsShuttingDown(true);
  EXPECT_TRUE(IsProcessShuttingDown());

  registry.SetIsShuttingDown(false);
  EXPECT_FALSE(IsProcessShuttingDown());
}

TEST(ProcessShutdownTest, OverrideForTesting) {
  ASSERT_FALSE(IsProcessShuttingDown());

  {
    ScopedProcessShuttingDownOverrideForTesting shutting_down_override;
    EXPECT_TRUE(IsProcessShuttingDown());
  }

  EXPECT_FALSE(IsProcessShuttingDown());
}

}  // namespace ash
