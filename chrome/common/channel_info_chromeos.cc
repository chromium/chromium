// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "chromeos/ash/components/channel/channel_info.h"
#include "components/version_info/version_info.h"

namespace chrome {

std::string GetChannelName(WithExtendedStable with_extended_stable) {
  return ash::GetChannelName();
}

version_info::Channel GetChannel() {
  return ash::GetChannel();
}

bool IsExtendedStableChannel() {
  return false;  // Not supported on Chrome OS Ash.
}

std::string GetChannelSuffixForDataDir() {
  // ChromeOS doesn't support side-by-side installations.
  return std::string();
}

}  // namespace chrome
