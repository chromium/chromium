// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/channel_to_enum.h"

#include "components/version_info/version_info.h"
#include "crosapi_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChannelToEnumTest, Lts) {
  EXPECT_EQ(crosapi::ChannelToEnum(crosapi::kReleaseChannelLts),
            version_info::Channel::STABLE);
}

TEST(ChannelToEnumTest, Ltc) {
  EXPECT_EQ(crosapi::ChannelToEnum(crosapi::kReleaseChannelLtc),
            version_info::Channel::STABLE);
}

TEST(ChannelToEnumTest, Stable) {
  EXPECT_EQ(crosapi::ChannelToEnum(crosapi::kReleaseChannelStable),
            version_info::Channel::STABLE);
}

TEST(ChannelToEnumTest, Beta) {
  EXPECT_EQ(crosapi::ChannelToEnum(crosapi::kReleaseChannelBeta),
            version_info::Channel::BETA);
}

TEST(ChannelToEnumTest, Dev) {
  EXPECT_EQ(crosapi::ChannelToEnum(crosapi::kReleaseChannelDev),
            version_info::Channel::DEV);
}

TEST(ChannelToEnumTest, Canary) {
  EXPECT_EQ(crosapi::ChannelToEnum(crosapi::kReleaseChannelCanary),
            version_info::Channel::CANARY);
}

TEST(ChannelToEnumTest, Unknown) {
  EXPECT_EQ(crosapi::ChannelToEnum("my-channel"),
            version_info::Channel::UNKNOWN);
}
