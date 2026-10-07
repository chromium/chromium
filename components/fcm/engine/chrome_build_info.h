// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_ENGINE_CHROME_BUILD_INFO_H_
#define COMPONENTS_FCM_ENGINE_CHROME_BUILD_INFO_H_

#include <string>

#include "components/version_info/channel.h"

namespace checkin_proto {
class ChromeBuildProto;
}  // namespace checkin_proto

namespace fcm {

enum class ChromePlatform {
  kWin,
  kMac,
  kLinux,
  kCros,
  kIos,
  kAndroid,
  kUnspecified,
};

enum class ChromeChannel {
  kStable,
  kBeta,
  kDev,
  kCanary,
  kUnknown,
};

struct ChromeBuildInfo {
  ChromePlatform platform = ChromePlatform::kUnspecified;
  ChromeChannel channel = ChromeChannel::kUnknown;
  std::string version;
  std::string product_category_for_subtypes;
};

// Constructs and returns a populated ChromeBuildInfo for the current build.
ChromeBuildInfo GetChromeBuildInfo(
    version_info::Channel channel,
    const std::string& product_category_for_subtypes);

// Converts `chrome_build_info` to the `android_build_info` protobuf format
// used in checkin requests.
void ToCheckinProtoVersion(const ChromeBuildInfo& chrome_build_info,
                           checkin_proto::ChromeBuildProto* android_build_info);

}  // namespace fcm

#endif  // COMPONENTS_FCM_ENGINE_CHROME_BUILD_INFO_H_
