// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/user_agent.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chromecast/base/version.h"
#include "chromecast/chromecast_buildflags.h"
#include "components/cast/common/constants.h"
#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "third_party/blink/public/common/features.h"

namespace chromecast {

namespace {

#if BUILDFLAG(IS_ANDROID)
std::string BuildAndroidOsInfo() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

  std::string android_version_str;
  base::StringAppendF(&android_version_str, "%d.%d", os_major_version,
                      os_minor_version);
  if (os_bugfix_version != 0)
    base::StringAppendF(&android_version_str, ".%d", os_bugfix_version);

  std::string android_info_str;
  // Append the build ID.
  std::string android_build_id = base::SysInfo::GetAndroidBuildID();
  if (android_build_id.size() > 0)
    android_info_str += "; Build/" + android_build_id;

  std::string os_info;
  base::StringAppendF(&os_info, "Android %s%s", android_version_str.c_str(),
                      android_info_str.c_str());
  return os_info;
}
#endif

std::string GetChromeKeyString() {
  std::string chrome_key = base::StrCat({"CrKey/", kFrozenCrKeyValue});
  return chrome_key;
}

std::string GetDeviceUserAgentSuffix() {
  return std::string(DEVICE_USER_AGENT_SUFFIX);
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
  base::StringAppendF(&os_info, "%s%s",
#if BUILDFLAG(IS_ANDROID)
                      "Linux; ", BuildAndroidOsInfo().c_str()
#else
                      "X11; ",
                      content::BuildOSCpuInfo(
                          content::IncludeAndroidBuildNumber::Exclude,
                          content::IncludeAndroidModel::Include)
                          .c_str()
#endif
  );
  return content::BuildUserAgentFromOSAndProduct(os_info, product);
}

}  // namespace

std::string GetUserAgent() {
  std::string chromium_user_agent = GetChromiumUserAgent();
  return base::StrCat({chromium_user_agent, " ", GetChromeKeyString(), " ",
                       GetDeviceUserAgentSuffix()});
}

}  // namespace chromecast
