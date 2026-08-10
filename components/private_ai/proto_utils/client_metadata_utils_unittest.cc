// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/proto_utils/client_metadata_utils.h"

#include <string>

#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {
namespace {

TEST(ClientMetadataUtilsTest, ConvertChannelToProto) {
  EXPECT_EQ(ConvertChannelToProto(version_info::Channel::STABLE),
            proto::ChromeClientMetadata::CHANNEL_STABLE);
  EXPECT_EQ(ConvertChannelToProto(version_info::Channel::BETA),
            proto::ChromeClientMetadata::CHANNEL_BETA);
  EXPECT_EQ(ConvertChannelToProto(version_info::Channel::DEV),
            proto::ChromeClientMetadata::CHANNEL_DEV);
  EXPECT_EQ(ConvertChannelToProto(version_info::Channel::CANARY),
            proto::ChromeClientMetadata::CHANNEL_CANARY);
  EXPECT_EQ(ConvertChannelToProto(version_info::Channel::UNKNOWN),
            proto::ChromeClientMetadata::CHANNEL_UNKNOWN);
}

TEST(ClientMetadataUtilsTest, GetPlatformForProto) {
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(GetPlatformForProto(),
            proto::ChromeClientMetadata::PLATFORM_WINDOWS);
#elif BUILDFLAG(IS_MAC)
  EXPECT_EQ(GetPlatformForProto(), proto::ChromeClientMetadata::PLATFORM_MAC);
#elif BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GetPlatformForProto(),
            proto::ChromeClientMetadata::PLATFORM_CHROMEOS);
#elif BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(GetPlatformForProto(),
            proto::ChromeClientMetadata::PLATFORM_ANDROID);
#elif BUILDFLAG(IS_IOS)
  EXPECT_EQ(GetPlatformForProto(), proto::ChromeClientMetadata::PLATFORM_IOS);
#elif BUILDFLAG(IS_LINUX)
  EXPECT_EQ(GetPlatformForProto(), proto::ChromeClientMetadata::PLATFORM_LINUX);
#else
  EXPECT_EQ(GetPlatformForProto(),
            proto::ChromeClientMetadata::PLATFORM_UNKNOWN);
#endif
}

TEST(ClientMetadataUtilsTest, CreateClientMetadata) {
  proto::ClientMetadata metadata =
      CreateClientMetadata(version_info::Channel::BETA);

  EXPECT_TRUE(metadata.has_chrome_client_metadata());
  const proto::ChromeClientMetadata& chrome_metadata =
      metadata.chrome_client_metadata();

  EXPECT_EQ(chrome_metadata.product_name(),
            std::string(version_info::GetProductName()));
  EXPECT_EQ(chrome_metadata.platform(), GetPlatformForProto());
  EXPECT_EQ(chrome_metadata.channel(),
            proto::ChromeClientMetadata::CHANNEL_BETA);
}

}  // namespace
}  // namespace private_ai
