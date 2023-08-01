// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#import <Foundation/Foundation.h>

#include <tuple>

#include "base/apple/bundle_locations.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/version_info/version_info.h"

namespace chrome {

namespace {

struct ChannelState {
  std::string name;
  bool is_extended_stable;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Returns a ChannelState given a KSChannelID value.
ChannelState ParseChannelId(NSString* channel) {
  // KSChannelID values:
  //
  //                     Intel       Arm              Universal
  //                   ┌───────────┬────────────────┬────────────────────┐
  //  Stable           │ (not set) │ arm64          │ universal          │
  //  Extended Stable  │ extended  │ arm64-extended │ universal-extended │
  //  Beta             │ beta      │ arm64-beta     │ universal-beta     │
  //  Dev              │ dev       │ arm64-dev      │ universal-dev      │
  //  Canary           │ canary    │ arm64-canary   │ universal-canary   │
  //                   └───────────┴────────────────┴────────────────────┘

  if (!channel || [channel isEqual:@"arm64"] ||
      [channel isEqual:@"universal"]) {
    return ChannelState{"", false};  // "" means stable channel.
  }

  if ([channel hasPrefix:@"arm64-"])
    channel = [channel substringFromIndex:[@"arm64-" length]];
  else if ([channel hasPrefix:@"universal-"])
    channel = [channel substringFromIndex:[@"universal-" length]];

  if ([channel isEqual:@"extended"])
    return ChannelState{"", true};  // "" means stable channel.

  if ([channel isEqual:@"beta"] || [channel isEqual:@"dev"] ||
      [channel isEqual:@"canary"]) {
    return ChannelState{base::SysNSStringToUTF8(channel), false};
  }

  return ChannelState{"unknown", false};
}

// Returns the ChannelState for this browser based on how it is registered with
// Keystone.
ChannelState DetermineChannelState() {
  // Use the main Chrome application bundle and not the framework bundle.
  // Keystone keys don't live in the framework.
  NSBundle* bundle = base::apple::OuterBundle();

  if (![bundle objectForInfoDictionaryKey:@"KSProductID"]) {
    // This build is not Keystone-enabled; it can't have a channel.
    return ChannelState{"unknown", false};
  }

  return ParseChannelId([bundle objectForInfoDictionaryKey:@"KSChannelID"]);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// For a branded build, returns its ChannelState: `name` of "" (for stable or
// extended), "beta", "dev", "canary", or "unknown" (in the case where the
// channel could not be determined or is otherwise inapplicable), and an
// `is_extended_stable` with a value corresponding to whether it is an extended
// stable build or not.
//
// For an unbranded build, always returns a ChannelState with `name` of "" and
// `is_extended_stable` of false.
ChannelState& GetChannelState() {
  static base::NoDestructor<ChannelState> channel([] {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    return DetermineChannelState();
#else
    return ChannelState{"", false};
#endif
  }());
  return *channel;
}

bool SideBySideCapable() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static const bool capable = [] {
    // Use the main Chrome application bundle and not the framework bundle.
    // Keystone keys don't live in the framework.
    NSBundle* bundle = base::apple::OuterBundle();
    if (![bundle objectForInfoDictionaryKey:@"KSProductID"]) {
      // This build is not Keystone-enabled, and without a channel assume it is
      // side-by-side capable.
      return true;
    }

    if (GetChannelState().name.empty()) {
      // GetChannelState() returns an empty name for the regular and extended
      // stable channels. These stable Chromes are what side-by-side capable
      // Chromes are running side-by-side *to* and by definition are
      // side-by-side capable.
      return true;
    }

    // If there is a CrProductDirName key, then the user data dir of this
    // beta/dev/canary Chrome is separate, and it can run side-by-side to the
    // stable Chrome.
    return [bundle objectForInfoDictionaryKey:@"CrProductDirName"] != nil;
  }();
  return capable;
#else
  return true;
#endif
}

}  // namespace

void CacheChannelInfo() {
  std::ignore = GetChannelState();
  std::ignore = SideBySideCapable();
}

std::string GetChannelName(WithExtendedStable with_extended_stable) {
  const auto& channel = GetChannelState();

  if (channel.is_extended_stable && with_extended_stable.value())
    return "extended";

  return channel.name;
}

version_info::Channel GetChannelByName(const std::string& channel) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (channel.empty() || channel == "extended")
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
  return SideBySideCapable();
}

version_info::Channel GetChannel() {
  return GetChannelByName(GetChannelState().name);
}

bool IsExtendedStableChannel() {
  return GetChannelState().is_extended_stable;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

// True following a call to SetChannelIdForTesting.
bool channel_id_is_overidden_ = false;

}  // namespace

void SetChannelIdForTesting(const std::string& channel_id) {
  DCHECK(!channel_id_is_overidden_);
  GetChannelState() = ParseChannelId(
      channel_id.empty() ? nullptr : base::SysUTF8ToNSString(channel_id));
  channel_id_is_overidden_ = true;
}

void ClearChannelIdForTesting() {
  DCHECK(channel_id_is_overidden_);
  channel_id_is_overidden_ = false;
  GetChannelState() = DetermineChannelState();
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace chrome
