// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace variations {

ClientFilterableState::ClientFilterableState(
    IsEnterpriseFunction is_enterprise_function,
    GoogleGroupsFunction google_groups_function)
    : is_enterprise_function_(std::move(is_enterprise_function)),
      google_groups_function_(std::move(google_groups_function)) {
  // The callback is only used when processing a study that uses the
  // is_enterprise filter. If you're building a client that isn't expecting that
  // filter, you should use a callback that always returns false.
  DCHECK(is_enterprise_function_);
}
ClientFilterableState::~ClientFilterableState() = default;

bool ClientFilterableState::IsEnterprise() const {
  if (!is_enterprise_.has_value()) {
    is_enterprise_ = std::move(is_enterprise_function_).Run();
  }
  return is_enterprise_.value();
}

base::flat_set<uint64_t> ClientFilterableState::GoogleGroups() const {
  if (!google_groups_.has_value()) {
    google_groups_ = std::move(google_groups_function_).Run();
  }
  return google_groups_.value();
}

// static
Study::Platform ClientFilterableState::GetCurrentPlatform() {
#if BUILDFLAG(IS_WIN)
  return Study::PLATFORM_WINDOWS;
#elif BUILDFLAG(IS_IOS)
  return Study::PLATFORM_IOS;
#elif BUILDFLAG(IS_MAC)
  return Study::PLATFORM_MAC;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return Study::PLATFORM_CHROMEOS;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return Study::PLATFORM_CHROMEOS_LACROS;
#elif BUILDFLAG(IS_ANDROID)
  return Study::PLATFORM_ANDROID;
#elif BUILDFLAG(IS_FUCHSIA)
  return Study::PLATFORM_FUCHSIA;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_BSD) || BUILDFLAG(IS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return Study::PLATFORM_LINUX;
#else
#error Unknown platform
#endif
}

// static
base::Version ClientFilterableState::GetOSVersion() {
  base::Version ret;

#if BUILDFLAG(IS_WIN)
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

std::string ClientFilterableState::GetHardwareClass() {
  // TODO(crbug.com/40708998): Expand to other platforms.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
  return base::SysInfo::HardwareModelName();
#else
  return "";
#endif
}

}  // namespace variations
