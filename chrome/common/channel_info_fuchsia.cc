// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1231926): Implement support for update channels.

#include "chrome/common/channel_info.h"

#include "base/notreached.h"
#include "components/version_info/version_info.h"

namespace chrome {

std::string GetChannelName(WithExtendedStable with_extended_stable) {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

std::string GetChannelSuffixForDataDir() {
  return {};
}

version_info::Channel GetChannel() {
  NOTIMPLEMENTED_LOG_ONCE();
  return version_info::Channel::STABLE;
}

bool IsExtendedStableChannel() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

}  // namespace chrome
