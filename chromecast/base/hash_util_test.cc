// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/hash_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace receiver {

TEST(UmaHashUtilTest, HashCastBuildNumber64) {
  EXPECT_EQ(0xffffffffffffffffU, HashCastBuildNumber64(""));

  EXPECT_EQ(1234567890U, HashCastBuildNumber64("1234567890"));

  EXPECT_EQ(0x0000000200000001U, HashCastBuildNumber64("2.1"));

  EXPECT_EQ(0x0002000000010000U, HashCastBuildNumber64("2.0.65536"));

  EXPECT_EQ(0x00020000007b03e7U, HashCastBuildNumber64("2.0.123.999"));

  EXPECT_EQ(0xffffffffffffffffU, HashCastBuildNumber64("2.na.123.invalid"));

  EXPECT_EQ(0xffffffffffffffffU, HashCastBuildNumber64("invalid"));
}

TEST(UmaHashUtilTest, HashSdkVersion64) {
  EXPECT_EQ(0UL, HashSdkVersion64(""));

  EXPECT_EQ(0x00020000007b0000U, HashSdkVersion64("2.0.123"));

  EXPECT_EQ(0x00020000007b03e7U, HashSdkVersion64("2.0.123.999"));

  EXPECT_EQ(0xffffffffffffffffU, HashSdkVersion64("2.na.123.invalid"));

  EXPECT_EQ(0xffffffffffffffffU, HashSdkVersion64("invalid"));
}

TEST(UmaHashUtilTest, HashAndroidBuildNumber64) {
  EXPECT_EQ(0x00000000004E5943U, HashAndroidBuildNumber64("NYC"));

  EXPECT_EQ(0x00004E554632364EU, HashAndroidBuildNumber64("NUF26N"));

  EXPECT_EQ(0x00004e524439304dU, HashAndroidBuildNumber64("NRD90M"));

  EXPECT_EQ(0x4d4d423239562e53U, HashAndroidBuildNumber64("MMB29V.S39"));

  EXPECT_EQ(0x4f5052312e313730U, HashAndroidBuildNumber64("OPR1.170508.001"));

  EXPECT_EQ(0x00004d4153544552U, HashAndroidBuildNumber64("MASTER"));

  EXPECT_EQ(0x00696e76616c6964U, HashAndroidBuildNumber64("invalid"));

  EXPECT_EQ(0xffffffffffffffffU, HashAndroidBuildNumber64(""));
}

}  // namespace receiver
}  // namespace chromecast
