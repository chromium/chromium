// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/channel/channel_to_enum.h"

#include "chromeos/ash/components/channel/channel_info.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(ChannelToEnumTest, Lts) {
  EXPECT_EQ(ChannelToEnum(kReleaseChannelLts), version_info::Channel::STABLE);
}

TEST(ChannelToEnumTest, Ltc) {
  EXPECT_EQ(ChannelToEnum(kReleaseChannelLtc), version_info::Channel::STABLE);
}

TEST(ChannelToEnumTest, Stable) {
  EXPECT_EQ(ChannelToEnum(kReleaseChannelStable),
            version_info::Channel::STABLE);
}

TEST(ChannelToEnumTest, Beta) {
  EXPECT_EQ(ChannelToEnum(kReleaseChannelBeta), version_info::Channel::BETA);
}

TEST(ChannelToEnumTest, Dev) {
  EXPECT_EQ(ChannelToEnum(kReleaseChannelDev), version_info::Channel::DEV);
}

TEST(ChannelToEnumTest, Canary) {
  EXPECT_EQ(ChannelToEnum(kReleaseChannelCanary),
            version_info::Channel::CANARY);
}

TEST(ChannelToEnumTest, Unknown) {
  EXPECT_EQ(ChannelToEnum("my-channel"), version_info::Channel::UNKNOWN);
}

}  // namespace ash
