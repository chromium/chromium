// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_channel_override.h"

#include <string>

#include "chrome/common/channel_info.h"

namespace chrome {

namespace {

std::string GetChannelId(ScopedChannelOverride::Channel channel) {
  switch (channel) {
    case ScopedChannelOverride::Channel::kExtendedStable:
      return "extended";
    case ScopedChannelOverride::Channel::kStable:
      return std::string();
    case ScopedChannelOverride::Channel::kBeta:
      return "beta";
    case ScopedChannelOverride::Channel::kDev:
      return "dev";
    case ScopedChannelOverride::Channel::kCanary:
      return "canary";
  }
}

}  // namespace

ScopedChannelOverride::ScopedChannelOverride(Channel channel) {
  SetChannelIdForTesting(GetChannelId(channel));
}

ScopedChannelOverride::~ScopedChannelOverride() {
  ClearChannelIdForTesting();
}

}  // namespace chrome
