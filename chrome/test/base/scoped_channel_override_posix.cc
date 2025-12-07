// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_channel_override.h"

#include "base/test/nix/scoped_chrome_version_extra_override.h"
#include "base/version_info/channel.h"
#include "base/version_info/nix/version_extra_utils.h"

namespace chrome {

namespace {

version_info::Channel GetBaseChannel(ScopedChannelOverride::Channel channel) {
  switch (channel) {
    case ScopedChannelOverride::Channel::kExtendedStable:
      return version_info::Channel::STABLE;
    case ScopedChannelOverride::Channel::kStable:
      return version_info::Channel::STABLE;
    case ScopedChannelOverride::Channel::kBeta:
      return version_info::Channel::BETA;
    case ScopedChannelOverride::Channel::kDev:
      return version_info::Channel::DEV;
#if BUILDFLAG(IS_LINUX)
    case ScopedChannelOverride::Channel::kCanary:
      return version_info::Channel::CANARY;
#endif  // BUILDFLAG(IS_LINUX)
  }
}

}  // namespace

ScopedChannelOverride::ScopedChannelOverride(Channel channel)
    : scoped_channel_override_(
          GetBaseChannel(channel),
          channel == ScopedChannelOverride::Channel::kExtendedStable) {}

ScopedChannelOverride::~ScopedChannelOverride() = default;

}  // namespace chrome
