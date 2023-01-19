// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/channel_to_enum.h"

namespace crosapi {

version_info::Channel ChannelToEnum(base::StringPiece channel) {
  if (channel == kReleaseChannelStable) {
    return version_info::Channel::STABLE;
  } else if (channel == kReleaseChannelBeta) {
    return version_info::Channel::BETA;
  } else if (channel == kReleaseChannelDev) {
    return version_info::Channel::DEV;
  } else if (channel == kReleaseChannelCanary) {
    return version_info::Channel::CANARY;
  } else {
    return version_info::Channel::UNKNOWN;
  }
}

}  // namespace crosapi
