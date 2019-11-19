// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "components/version_info/version_info.h"

namespace chrome {
namespace {

version_info::Channel g_chromeos_channel = version_info::Channel::UNKNOWN;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Sets the |g_chromeos_channel|.
void SetChannel(const std::string& channel) {
  if (channel == "stable-channel")
    g_chromeos_channel = version_info::Channel::STABLE;
  else if (channel == "beta-channel")
    g_chromeos_channel = version_info::Channel::BETA;
  else if (channel == "dev-channel")
    g_chromeos_channel = version_info::Channel::DEV;
  else if (channel == "canary-channel")
    g_chromeos_channel = version_info::Channel::CANARY;
  else
    g_chromeos_channel = version_info::Channel::UNKNOWN;
}
#endif

}  // namespace

std::string GetChannelName() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (GetChannel()) {
    case version_info::Channel::STABLE:
      return std::string();
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
    return g_chromeos_channel;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &channel)) {
    SetChannel(channel);
    is_channel_set = true;
  }
#endif
  return g_chromeos_channel;
}

std::string GetChannelSuffixForDataDir() {
  // ChromeOS doesn't support side-by-side installations.
  return std::string();
}

}  // namespace chrome
