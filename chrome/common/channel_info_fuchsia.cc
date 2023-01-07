// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1231926): Implement support for update channels.

#include "chrome/common/channel_info.h"

#include "base/check.h"
#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "components/version_info/version_info.h"

namespace chrome {

namespace {

struct ChannelState {
  version_info::Channel channel;
  bool is_extended_stable;
};

// Determine the state of the browser based on branding and channel.
// TODO(crbug.com/1253820): Update implementation when channel are implemented.
ChannelState DetermineChannelState() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  NOTIMPLEMENTED_LOG_ONCE();
  return {version_info::Channel::STABLE, /*is_extended_stable=*/false};
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return {version_info::Channel::UNKNOWN, /*is_extended_stable=*/false};
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// Returns the channel state for the browser.
ChannelState& GetChannelImpl() {
  static ChannelState channel = DetermineChannelState();
  return channel;
}

}  // namespace

std::string GetChannelName(WithExtendedStable with_extended_stable) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto& channel_state = GetChannelImpl();
  switch (channel_state.channel) {
    case version_info::Channel::UNKNOWN:
      return "unknown";
    case version_info::Channel::CANARY:
      return "canary";
    case version_info::Channel::DEV:
      return "dev";
    case version_info::Channel::BETA:
      return "beta";
    case version_info::Channel::STABLE:
      if (with_extended_stable && channel_state.is_extended_stable)
        return "extended";
      return std::string();
  }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return std::string();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

version_info::Channel GetChannel() {
  return GetChannelImpl().channel;
}

bool IsExtendedStableChannel() {
  return GetChannelImpl().is_extended_stable;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void SetChannelForTesting(version_info::Channel channel,
                          bool is_extended_stable) {
  GetChannelImpl() = {channel, is_extended_stable};
}

void ClearChannelForTesting() {
  GetChannelImpl() = DetermineChannelState();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace chrome
