// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/engine/chrome_build_info.h"

#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fcm {
namespace {

TEST(ChromeBuildInfoTest, ToCheckinProtoVersion) {
  ChromeBuildInfo info;
  info.platform = ChromePlatform::kMac;
  info.channel = ChromeChannel::kBeta;
  info.version = "130.0.1.0";
  info.product_category_for_subtypes = "com.chrome.macosx";

  checkin_proto::ChromeBuildProto proto;
  ToCheckinProtoVersion(info, &proto);

  EXPECT_EQ(checkin_proto::ChromeBuildProto_Platform_PLATFORM_MAC,
            proto.platform());
  EXPECT_EQ(checkin_proto::ChromeBuildProto_Channel_CHANNEL_BETA,
            proto.channel());
  EXPECT_EQ("130.0.1.0", proto.chrome_version());
}

TEST(ChromeBuildInfoTest, UnspecifiedPlatformDefaultsToLinux) {
  ChromeBuildInfo info;
  info.platform = ChromePlatform::kUnspecified;
  info.channel = ChromeChannel::kStable;
  info.version = "130.0.0.0";

  checkin_proto::ChromeBuildProto proto;
  ToCheckinProtoVersion(info, &proto);

  EXPECT_EQ(checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX,
            proto.platform());
  EXPECT_EQ(checkin_proto::ChromeBuildProto_Channel_CHANNEL_STABLE,
            proto.channel());
}

}  // namespace
}  // namespace fcm
