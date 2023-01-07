// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_channel_override.h"

#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace chrome {

namespace {

version_info::Channel GetChannel(ScopedChannelOverride::Channel channel) {
  switch (channel) {
    case ScopedChannelOverride::Channel::kExtendedStable:
    case ScopedChannelOverride::Channel::kStable:
      return version_info::Channel::STABLE;
    case ScopedChannelOverride::Channel::kBeta:
      return version_info::Channel::BETA;
    case ScopedChannelOverride::Channel::kDev:
      return version_info::Channel::DEV;
    case ScopedChannelOverride::Channel::kCanary:
      return version_info::Channel::CANARY;
  }
}

}  // namespace

ScopedChannelOverride::ScopedChannelOverride(Channel channel) {
  SetChannelForTesting(GetChannel(channel),
                       channel == Channel::kExtendedStable);
}

ScopedChannelOverride::~ScopedChannelOverride() {
  ClearChannelForTesting();
}

}  // namespace chrome
