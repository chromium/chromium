// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#import <Foundation/Foundation.h>

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/version_info/version_info.h"

namespace chrome {

std::string GetChannelName() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Use the main Chrome application bundle and not the framework bundle.
  // Keystone keys don't live in the framework.
  NSBundle* bundle = base::mac::OuterBundle();
  NSString* channel = [bundle objectForInfoDictionaryKey:@"KSChannelID"];

  // Only ever return "", "unknown", "beta", "dev", or "canary" in a branded
  // build.
  // KSProductID is not set (for stable) or "beta", "dev" or "canary" for
  // the intel-only build.
  // KSProductID is "arm64" (for stable) or "arm64-beta", "arm64-dev" or
  // "arm64-canary" for the arm-only build.
  // KSProductID is "universal" (for stable) or "universal-beta",
  // "universal-dev" or "universal-canary" for the arm+intel universal binary.
  if (![bundle objectForInfoDictionaryKey:@"KSProductID"]) {
    // This build is not Keystone-enabled, it can't have a channel.
    channel = @"unknown";
  } else if (!channel || [channel isEqual:@"arm64"] ||
             [channel isEqual:@"universal"]) {
    // For the intel stable channel, KSChannelID is not set.
    channel = @"";
  } else {
    if ([channel hasPrefix:@"arm64-"])
      channel = [channel substringFromIndex:[@"arm64-" length]];
    else if ([channel hasPrefix:@"universal-"])
      channel = [channel substringFromIndex:[@"universal-" length]];
    if ([channel isEqual:@"beta"] || [channel isEqual:@"dev"] ||
        [channel isEqual:@"canary"]) {
      // do nothing.
    } else {
      channel = @"unknown";
    }
  }

  return base::SysNSStringToUTF8(channel);
#else
  return std::string();
#endif
}

version_info::Channel GetChannelByName(const std::string& channel) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (channel.empty())
    return version_info::Channel::STABLE;
  if (channel == "beta")
    return version_info::Channel::BETA;
  if (channel == "dev")
    return version_info::Channel::DEV;
  if (channel == "canary")
    return version_info::Channel::CANARY;
#endif
  return version_info::Channel::UNKNOWN;
}

bool IsSideBySideCapable() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Use the main Chrome application bundle and not the framework bundle.
  // Keystone keys don't live in the framework.
  NSBundle* bundle = base::mac::OuterBundle();
  if (![bundle objectForInfoDictionaryKey:@"KSProductID"]) {
    // This build is not Keystone-enabled, and without a channel assume it is
    // side-by-side capable.
    return true;
  }

  if (GetChannelName().empty()) {
    // For the stable channel, GetChannelName() returns the empty string.
    // Stable Chromes are what side-by-side capable Chromes are running
    // side-by-side *to* and by definition are side-by-side capable.
    return true;
  }

  // If there is a CrProductDirName key, then the user data dir of this
  // beta/dev/canary Chrome is separate, and it can run side-by-side to the
  // stable Chrome.
  return [bundle objectForInfoDictionaryKey:@"CrProductDirName"];
#else
  return true;
#endif
}

version_info::Channel GetChannel() {
  return GetChannelByName(GetChannelName());
}

}  // namespace chrome
