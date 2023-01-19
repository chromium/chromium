// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/crosapi/cpp/channel_to_enum.h"
#endif

namespace chrome {
namespace {

version_info::Channel g_chromeos_channel = version_info::Channel::UNKNOWN;

}  // namespace

std::string GetChannelName(WithExtendedStable with_extended_stable) {
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
#else
  return std::string();
#endif
}

version_info::Channel GetChannel() {
  static bool is_channel_set = false;
  if (is_channel_set)
    return g_chromeos_channel;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(crosapi::kChromeOSReleaseTrack,
                                        &channel)) {
    g_chromeos_channel = crosapi::ChannelToEnum(channel);
    is_channel_set = true;
  }
#endif
  return g_chromeos_channel;
}

bool IsExtendedStableChannel() {
  return false;  // Not supported on Chrome OS Ash.
}

std::string GetChannelSuffixForDataDir() {
  // ChromeOS doesn't support side-by-side installations.
  return std::string();
}

}  // namespace chrome
