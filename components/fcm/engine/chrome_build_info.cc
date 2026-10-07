// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/engine/chrome_build_info.h"

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"

namespace fcm {

namespace {

ChromePlatform GetPlatform() {
#if BUILDFLAG(IS_WIN)
  return ChromePlatform::kWin;
#elif BUILDFLAG(IS_APPLE)
  // TODO(b/570485572): Distinguish between iOS and macOS.
  return ChromePlatform::kMac;
#elif BUILDFLAG(IS_IOS)
  return ChromePlatform::kIos;
#elif BUILDFLAG(IS_ANDROID)
  return ChromePlatform::kAndroid;
#elif BUILDFLAG(IS_CHROMEOS)
  return ChromePlatform::kCros;
#elif BUILDFLAG(IS_LINUX)
  return ChromePlatform::kLinux;
#else
  // For all other platforms, return as LINUX.
  return ChromePlatform::kLinux;
#endif
}

ChromeChannel GetChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return ChromeChannel::kUnknown;
    case version_info::Channel::CANARY:
      return ChromeChannel::kCanary;
    case version_info::Channel::DEV:
      return ChromeChannel::kDev;
    case version_info::Channel::BETA:
      return ChromeChannel::kBeta;
    case version_info::Channel::STABLE:
      return ChromeChannel::kStable;
  }
  NOTREACHED();
}

std::string GetVersion() {
  return std::string(version_info::GetVersionNumber());
}

}  // namespace

ChromeBuildInfo GetChromeBuildInfo(
    version_info::Channel channel,
    const std::string& product_category_for_subtypes) {
  ChromeBuildInfo chrome_build_info;
  chrome_build_info.platform = GetPlatform();
  chrome_build_info.channel = GetChannel(channel);
  chrome_build_info.version = GetVersion();
  chrome_build_info.product_category_for_subtypes =
      product_category_for_subtypes;
  return chrome_build_info;
}

void ToCheckinProtoVersion(
    const ChromeBuildInfo& chrome_build_info,
    checkin_proto::ChromeBuildProto* android_build_info) {
  CHECK(android_build_info);

  checkin_proto::ChromeBuildProto_Platform platform =
      checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
  switch (chrome_build_info.platform) {
    case ChromePlatform::kWin:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_WIN;
      break;
    case ChromePlatform::kMac:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_MAC;
      break;
    case ChromePlatform::kLinux:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
      break;
    case ChromePlatform::kIos:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_IOS;
      break;
    case ChromePlatform::kAndroid:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_ANDROID;
      break;
    case ChromePlatform::kCros:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_CROS;
      break;
    case ChromePlatform::kUnspecified:
      // For unknown platform, return as LINUX.
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
      break;
  }
  android_build_info->set_platform(platform);

  checkin_proto::ChromeBuildProto_Channel channel =
      checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
  switch (chrome_build_info.channel) {
    case ChromeChannel::kStable:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_STABLE;
      break;
    case ChromeChannel::kBeta:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_BETA;
      break;
    case ChromeChannel::kDev:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_DEV;
      break;
    case ChromeChannel::kCanary:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_CANARY;
      break;
    case ChromeChannel::kUnknown:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
      break;
  }
  android_build_info->set_channel(channel);

  android_build_info->set_chrome_version(chrome_build_info.version);
}

}  // namespace fcm
