// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_USER_AGENT_H_
#define CHROMECAST_COMMON_USER_AGENT_H_

#include <string>

namespace chromecast {

std::string GetDeviceUserAgentSuffix();
std::string GetChromiumUserAgent();

std::string GetChromeKeyString();
std::string GetUserAgent();

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_USER_AGENT_H_
