// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/user_agent.h"

#include <string>
#include "base/strings/strcat.h"
#include "components/cast/common/constants.h"

namespace chromecast {

std::string GetChromeKeyString() {
  std::string chrome_key = base::StrCat({"CrKey/", kFrozenCrKeyValue});
  return chrome_key;
}
std::string GetUserAgent() {
  std::string chromium_user_agent = GetChromiumUserAgent();
  return base::StrCat({chromium_user_agent, " ", GetChromeKeyString(), " ",
                       GetDeviceUserAgentSuffix()});
}

}  // namespace chromecast
