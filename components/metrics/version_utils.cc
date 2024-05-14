// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/version_utils.h"

#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace metrics {

std::string GetVersionString() {
  std::string version(version_info::GetVersionNumber());
#if defined(ARCH_CPU_64_BITS)
  version += "-64";
#endif  // defined(ARCH_CPU_64_BITS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  bool is_chrome_branded = true;
#else
  bool is_chrome_branded = false;
#endif
  if (!is_chrome_branded || !version_info::IsOfficialBuild())
    version.append("-devel");
  return version;
}

SystemProfileProto::Channel AsProtobufChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return SystemProfileProto::CHANNEL_UNKNOWN;
    case version_info::Channel::CANARY:
      return SystemProfileProto::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return SystemProfileProto::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return SystemProfileProto::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return SystemProfileProto::CHANNEL_STABLE;
  }
  NOTREACHED_IN_MIGRATION();
  return SystemProfileProto::CHANNEL_UNKNOWN;
}

std::string GetAppPackageName() {
#if BUILDFLAG(IS_ANDROID)
  return base::android::BuildInfo::GetInstance()->package_name();
#else
  return std::string();
#endif
}

}  // namespace metrics
