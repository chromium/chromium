// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/sys_info.h"
#include "components/version_info/version_info.h"

namespace chrome {
namespace {

version_info::Channel chromeos_channel = version_info::Channel::UNKNOWN;

#if defined(GOOGLE_CHROME_BUILD)
// Sets the |chromeos_channel|.
void SetChannel(const std::string& channel) {
  if (channel == "stable-channel") {
    chromeos_channel = version_info::Channel::STABLE;
  } else if (channel == "beta-channel") {
    chromeos_channel = version_info::Channel::BETA;
  } else if (channel == "dev-channel") {
    chromeos_channel = version_info::Channel::DEV;
  } else if (channel == "canary-channel") {
    chromeos_channel = version_info::Channel::CANARY;
  } else {
    chromeos_channel = version_info::Channel::UNKNOWN;
  }
}
#endif

}  // namespace

std::string GetChannelName() {
#if defined(GOOGLE_CHROME_BUILD)
  switch (chromeos_channel) {
    case version_info::Channel::STABLE:
      return "";
    case version_info::Channel::BETA:
      return "beta";
    case version_info::Channel::DEV:
      return "dev";
    case version_info::Channel::CANARY:
      return "canary";
    default:
      return "unknown";
  }
#endif
  return std::string();
}

version_info::Channel GetChannel() {
  static bool is_channel_set = false;
  if (is_channel_set)
    return chromeos_channel;

#if !defined(GOOGLE_CHROME_BUILD)
  return version_info::Channel::UNKNOWN;
#else
  static const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &channel)) {
    SetChannel(channel);
    is_channel_set = true;
  }
  return chromeos_channel;
#endif
}

#if defined(GOOGLE_CHROME_BUILD)
std::string GetChannelSuffixForDataDir() {
  // ChromeOS doesn't support side-by-side installations.
  return std::string();
}
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace chrome
