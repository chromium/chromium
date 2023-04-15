// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/user_agent.h"

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chromecast/base/version.h"
#include "chromecast/chromecast_buildflags.h"
#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "third_party/blink/public/common/features.h"

namespace chromecast {

std::string GetDeviceUserAgentSuffix() {
  return std::string(DEVICE_USER_AGENT_SUFFIX);
}

std::string GetChromiumUserAgent() {
  if (base::FeatureList::IsEnabled(blink::features::kReduceUserAgent)) {
    return content::GetReducedUserAgent(
        /*mobile=*/false, version_info::GetMajorVersionNumber());
  }

  std::string product = "Chrome/" PRODUCT_VERSION;
  std::string os_info;
  base::StringAppendF(
      &os_info, "%s%s", "X11; ",
      content::BuildOSCpuInfo(content::IncludeAndroidBuildNumber::Exclude,
                              content::IncludeAndroidModel::Include)
          .c_str());
  return content::BuildUserAgentFromOSAndProduct(os_info, product);
}

}  // namespace chromecast
