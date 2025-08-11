// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chromecast/common/user_agent.h"

#include "base/android/device_info.h"
#include "components/embedder_support/user_agent_utils.h"

namespace chromecast {

std::string GetDeviceUserAgentSuffix() {
  if (base::android::device_info::is_tv()) {
    return "DeviceType/AndroidTV";
  } else {
    return "DeviceType/Android";
  }
}

std::string GetChromiumUserAgent() {
  return embedder_support::GetUserAgent();
}

}  // namespace chromecast
