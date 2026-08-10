// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/proto_utils/client_metadata_utils.h"

#include <string>

#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

proto::ChromeClientMetadata::Channel ConvertChannelToProto(
    version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::STABLE:
      return proto::ChromeClientMetadata::CHANNEL_STABLE;
    case version_info::Channel::BETA:
      return proto::ChromeClientMetadata::CHANNEL_BETA;
    case version_info::Channel::DEV:
      return proto::ChromeClientMetadata::CHANNEL_DEV;
    case version_info::Channel::CANARY:
      return proto::ChromeClientMetadata::CHANNEL_CANARY;
    case version_info::Channel::UNKNOWN:
      return proto::ChromeClientMetadata::CHANNEL_UNKNOWN;
  }
  return proto::ChromeClientMetadata::CHANNEL_UNKNOWN;
}

proto::ChromeClientMetadata::Platform GetPlatformForProto() {
#if BUILDFLAG(IS_WIN)
  return proto::ChromeClientMetadata::PLATFORM_WINDOWS;
#elif BUILDFLAG(IS_MAC)
  return proto::ChromeClientMetadata::PLATFORM_MAC;
#elif BUILDFLAG(IS_CHROMEOS)
  return proto::ChromeClientMetadata::PLATFORM_CHROMEOS;
#elif BUILDFLAG(IS_ANDROID)
  return proto::ChromeClientMetadata::PLATFORM_ANDROID;
#elif BUILDFLAG(IS_IOS)
  return proto::ChromeClientMetadata::PLATFORM_IOS;
#elif BUILDFLAG(IS_LINUX)
  return proto::ChromeClientMetadata::PLATFORM_LINUX;
#else
  return proto::ChromeClientMetadata::PLATFORM_UNKNOWN;
#endif
}

proto::ClientMetadata CreateClientMetadata(version_info::Channel channel) {
  proto::ClientMetadata client_metadata;
  proto::ChromeClientMetadata* chrome_client_metadata =
      client_metadata.mutable_chrome_client_metadata();
  chrome_client_metadata->set_product_name(
      std::string(version_info::GetProductName()));
  chrome_client_metadata->set_platform(GetPlatformForProto());
  chrome_client_metadata->set_channel(ConvertChannelToProto(channel));
  return client_metadata;
}

}  // namespace private_ai
