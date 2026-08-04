// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_switches.h"

namespace variations {

ClientFilterableState::ClientFilterableState(
    IsEnterpriseFunction is_enterprise_function,
    GoogleGroupsFunction google_groups_function,
    EnterpriseGroupsFunction enterprise_groups_function)
    : is_enterprise_function_(std::move(is_enterprise_function)),
      google_groups_function_(std::move(google_groups_function)),
      enterprise_groups_function_(std::move(enterprise_groups_function)) {
  // The callback is only used when processing a study that uses the
  // is_enterprise filter. If you're building a client that isn't expecting that
  // filter, you should use a callback that always returns false.
  DCHECK(is_enterprise_function_);
}

ClientFilterableState::ClientFilterableState()
    : ClientFilterableState(
          base::BindOnce([] { return false; }),
          base::BindOnce([] { return base::flat_set<uint64_t>(); }),
          base::BindOnce([] { return base::flat_set<std::string>(); })) {}

ClientFilterableState::~ClientFilterableState() = default;

std::unique_ptr<ClientFilterableState>
ClientFilterableState::CreateWithIsEnterprise(
    IsEnterpriseFunction is_enterprise_function) {
  return std::make_unique<ClientFilterableState>(
      std::move(is_enterprise_function),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }),
      base::BindOnce([] { return base::flat_set<std::string>(); }));
}

std::unique_ptr<ClientFilterableState>
ClientFilterableState::CreateWithGoogleGroups(
    GoogleGroupsFunction google_groups_function) {
  return std::make_unique<ClientFilterableState>(
      base::BindOnce([] { return false; }), std::move(google_groups_function),
      base::BindOnce([] { return base::flat_set<std::string>(); }));
}

std::unique_ptr<ClientFilterableState>
ClientFilterableState::CreateWithEnterpriseGroups(
    EnterpriseGroupsFunction enterprise_groups_function) {
  return std::make_unique<ClientFilterableState>(
      base::BindOnce([] { return false; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }),
      std::move(enterprise_groups_function));
}

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

base::flat_set<std::string> ClientFilterableState::EnterpriseGroups() const {
  if (!enterprise_groups_.has_value()) {
    enterprise_groups_ = std::move(enterprise_groups_function_).Run();
  }
  return enterprise_groups_.value();
}

// static
Study::Platform ClientFilterableState::GetCurrentPlatform() {
  const std::string forced_platform =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFakeVariationsPlatform);
  if (!forced_platform.empty()) {
    if (forced_platform == "android") {
      return Study::PLATFORM_ANDROID;
    }
    if (forced_platform == "android_webview") {
      return Study::PLATFORM_ANDROID_WEBVIEW;
    }
    if (forced_platform == "chromeos") {
      return Study::PLATFORM_CHROMEOS;
    }
    if (forced_platform == "fuchsia") {
      return Study::PLATFORM_FUCHSIA;
    }
    if (forced_platform == "ios") {
      return Study::PLATFORM_IOS;
    }
    if (forced_platform == "linux") {
      return Study::PLATFORM_LINUX;
    }
    if (forced_platform == "mac") {
      return Study::PLATFORM_MAC;
    }
    if (forced_platform == "win") {
      return Study::PLATFORM_WINDOWS;
    }
    DVLOG(1) << "Invalid platform provided: " << forced_platform;
  }

#if BUILDFLAG(IS_WIN)
  return Study::PLATFORM_WINDOWS;
#elif BUILDFLAG(IS_IOS)
  return Study::PLATFORM_IOS;
#elif BUILDFLAG(IS_MAC)
  return Study::PLATFORM_MAC;
#elif BUILDFLAG(IS_CHROMEOS)
  return Study::PLATFORM_CHROMEOS;
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
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  return base::SysInfo::HardwareModelName();
#else
  return "";
#endif
}

std::string ClientFilterableState::GetHardwareManufacturer() {
#if BUILDFLAG(IS_ANDROID)
  return base::SysInfo::HardwareManufacturer();
#else
  return "";
#endif
}

}  // namespace variations
