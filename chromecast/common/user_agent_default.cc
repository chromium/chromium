// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/user_agent.h"

#include "chromecast/chromecast_buildflags.h"
#include "components/embedder_support/user_agent_utils.h"

namespace chromecast {

std::string GetDeviceUserAgentSuffix() {
  return std::string(DEVICE_USER_AGENT_SUFFIX);
}

std::string GetChromiumUserAgent() {
  return embedder_support::GetUserAgent();
}

}  // namespace chromecast
