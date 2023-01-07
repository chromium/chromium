// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(UpdateEngineClientTest, IsTargetChannelMoreStable) {
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("stable-channel",
                                                             "beta-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("beta-channel",
                                                             "dev-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("dev-channel",
                                                             "canary-channel"));
  EXPECT_TRUE(UpdateEngineClient::IsTargetChannelMoreStable("beta-channel",
                                                            "stable-channel"));
  EXPECT_TRUE(UpdateEngineClient::IsTargetChannelMoreStable("dev-channel",
                                                            "stable-channel"));
  EXPECT_TRUE(UpdateEngineClient::IsTargetChannelMoreStable("canary-channel",
                                                            "stable-channel"));
  EXPECT_TRUE(UpdateEngineClient::IsTargetChannelMoreStable("dev-channel",
                                                            "beta-channel"));
  EXPECT_TRUE(UpdateEngineClient::IsTargetChannelMoreStable("canary-channel",
                                                            "dev-channel"));
  // Staying on the same channel always returns false.
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("stable-channel",
                                                             "stable-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("beta-channel",
                                                             "beta-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("dev-channel",
                                                             "dev-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("canary-channel",
                                                             "canary-channel"));
  // Invalid channel names are considered more stable than valid ones, to be
  // consistent with the update_engine behavior.
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("invalid-channel",
                                                             "canary-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable("invalid-channel",
                                                             "stable-channel"));
  EXPECT_TRUE(UpdateEngineClient::IsTargetChannelMoreStable("canary-channel",
                                                            "invalid-channel"));
  EXPECT_TRUE(UpdateEngineClient::IsTargetChannelMoreStable("stable-channel",
                                                            "invalid-channel"));
  // If both channels are invalid, we return false.
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable(
      "invalid-channel", "invalid-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable(
      "invalid-channel", "other-invalid-channel"));
  EXPECT_FALSE(UpdateEngineClient::IsTargetChannelMoreStable(
      "other-invalid-channel", "invalid-channel"));
}

}  // namespace ash
