// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace variations {

// static
Study::Platform ClientFilterableState::GetCurrentPlatform() {
#if defined(OS_WIN)
  return Study::PLATFORM_WINDOWS;
#elif defined(OS_IOS)
  return Study::PLATFORM_IOS;
#elif defined(OS_APPLE)
  return Study::PLATFORM_MAC;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return Study::PLATFORM_CHROMEOS;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return Study::PLATFORM_CHROMEOS_LACROS;
#elif defined(OS_ANDROID)
  return Study::PLATFORM_ANDROID;
#elif defined(OS_FUCHSIA)
  return Study::PLATFORM_FUCHSIA;
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return Study::PLATFORM_LINUX;
#else
#error Unknown platform
#endif
}

// TODO(b/957197): Improve how we handle OS versions.
// Add os_version.h and os_version_<platform>.cc that handle retrieving and
// parsing OS versions. Then get rid of all the platform-dependent code here.
//
// static
base::Version ClientFilterableState::GetOSVersion() {
  base::Version ret;

#if defined(OS_WIN)
  std::string win_version = base::SysInfo::OperatingSystemVersion();
  base::ReplaceSubstringsAfterOffset(&win_version, 0, " SP", ".");
  ret = base::Version(win_version);
  DCHECK(ret.IsValid()) << win_version;
#else
  // Every other OS is supported by OperatingSystemVersionNumbers
  int major, minor, build;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &build);
  ret = base::Version(base::StringPrintf("%d.%d.%d", major, minor, build));
  DCHECK(ret.IsValid());
#endif

  return ret;
}

ClientFilterableState::ClientFilterableState(
    IsEnterpriseFunction is_enterprise_function)
    : is_enterprise_function_(std::move(is_enterprise_function)) {}
ClientFilterableState::~ClientFilterableState() {}

bool ClientFilterableState::IsEnterprise() const {
  if (!is_enterprise_.has_value())
    is_enterprise_ = std::move(is_enterprise_function_).Run();
  return is_enterprise_.value();
}

}  // namespace variations
