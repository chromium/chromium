// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/android/build_info.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"

namespace chrome {

std::string GetChannelName(WithExtendedStable with_extended_stable) {
  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN: return "unknown";
    case version_info::Channel::CANARY: return "canary";
    case version_info::Channel::DEV: return "dev";
    case version_info::Channel::BETA: return "beta";
    case version_info::Channel::STABLE: return std::string();
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown channel " << static_cast<int>(GetChannel());
  return std::string();
}

version_info::Channel GetChannel() {
  return version_info::android::GetChannel();
}

bool IsExtendedStableChannel() {
  return false;  // Not supported on Android.
}

}  // namespace chrome
