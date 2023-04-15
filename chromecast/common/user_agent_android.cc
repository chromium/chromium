// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chromecast/common/user_agent.h"

#include "base/android/build_info.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chromecast/base/version.h"
#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "third_party/blink/public/common/features.h"

namespace chromecast {

std::string BuildAndroidOsInfo() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

  std::string android_version_str;
  base::StringAppendF(&android_version_str, "%d.%d", os_major_version,
                      os_minor_version);
  if (os_bugfix_version != 0) {
    base::StringAppendF(&android_version_str, ".%d", os_bugfix_version);
  }

  std::string android_info_str;
  // Append the build ID.
  std::string android_build_id = base::SysInfo::GetAndroidBuildID();
  if (android_build_id.size() > 0) {
    android_info_str += "; Build/" + android_build_id;
  }

  std::string os_info;
  base::StringAppendF(&os_info, "Android %s%s", android_version_str.c_str(),
                      android_info_str.c_str());
  return os_info;
}

std::string GetDeviceUserAgentSuffix() {
  auto* build_info = base::android::BuildInfo::GetInstance();

  if (build_info->is_tv()) {
    return "DeviceType/AndroidTV";
  } else {
    return "DeviceType/Android";
  }
}

// TODO(guohuideng): Use embedder_support::GetUserAgent() instead after we
// decouple chromecast and the web browser, when we have fewer restrictions on
// gn target dependency.
std::string GetChromiumUserAgent() {
  if (base::FeatureList::IsEnabled(blink::features::kReduceUserAgent)) {
    return content::GetReducedUserAgent(
        /*mobile=*/false, version_info::GetMajorVersionNumber());
  }

  std::string product = "Chrome/" PRODUCT_VERSION;
  std::string os_info;
  base::StringAppendF(&os_info, "%s%s", "Linux; ",
                      BuildAndroidOsInfo().c_str());
  return content::BuildUserAgentFromOSAndProduct(os_info, product);
}

}  // namespace chromecast
