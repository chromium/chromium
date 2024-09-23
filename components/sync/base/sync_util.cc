// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_util.h"

#include <string_view>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringize_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/command_line_switches.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_config.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

namespace {

// Returns string that represents system in UserAgent.
std::string GetSystemString() {
  std::string system;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  system = "CROS ";
#elif BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    system = "ANDROID-TABLET ";
  } else {
    system = "ANDROID-PHONE ";
  }
#elif BUILDFLAG(IS_IOS)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    system = "IOS-TABLET ";
  } else {
    system = "IOS-PHONE ";
  }
#elif BUILDFLAG(IS_WIN)
  system = "WIN ";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  system = "LINUX ";
#elif BUILDFLAG(IS_FREEBSD)
  system = "FREEBSD ";
#elif BUILDFLAG(IS_OPENBSD)
  system = "OPENBSD ";
#elif BUILDFLAG(IS_MAC)
  system = "MAC ";
#endif
  return system;
}

}  // namespace

namespace syncer {
namespace internal {

std::string FormatUserAgentForSync(const std::string& system,
                                   version_info::Channel channel) {
#ifndef SYNC_USER_AGENT_PRODUCT
#error SYNC_USER_AGENT_PRODUCT not defined, check BUILD.gn.
#endif
  constexpr std::string_view kProduct = STRINGIZE(SYNC_USER_AGENT_PRODUCT);
  return base::StrCat(
      {kProduct, " ", system, version_info::GetVersionNumber(), " (",
       version_info::GetLastChange(), ")",
       version_info::IsOfficialBuild()
           ? base::StrCat(
                 {" channel(", version_info::GetChannelString(channel), ")"})
           : std::string("-devel")});
}

}  // namespace internal

GURL GetSyncServiceURL(const base::CommandLine& command_line,
                       version_info::Channel channel) {
  // Priorities for determining the sync URL:
  // 1. Explicitly specified --sync-url
  // 2. Specified as part of the --gaia-config
  // 3. Default URL (different for Stable/Beta vs. Dev/Canary/unbranded)

  // 1. Get the sync server URL from the --sync-url command-line param, if
  // specified.
  if (command_line.HasSwitch(kSyncServiceURL)) {
    std::string value(command_line.GetSwitchValueASCII(kSyncServiceURL));
    if (!value.empty()) {
      GURL custom_sync_url(value);
      if (custom_sync_url.is_valid()) {
        return custom_sync_url;
      } else {
        LOG(WARNING) << "The following sync URL specified at the command-line "
                     << "is invalid: " << value;
      }
    }
  }

  // 2. Get the sync server URL from the --gaia-config, if the config exists and
  // contains a sync URL.
  GaiaConfig* gaia_config = GaiaConfig::GetInstance();
  if (gaia_config) {
    GURL url;
    if (gaia_config->GetURLIfExists("sync_url", &url)) {
      return url;
    }
  }

  // 3. By default, dev, canary, and unbranded Chromium users will go to the
  // development servers. Development servers have more features than standard
  // sync servers. Users with officially-branded Chrome stable and beta builds
  // will go to the standard sync servers.
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::BETA) {
    return GURL(internal::kSyncServerUrl);
  }

  return GURL(internal::kSyncDevServerUrl);
}

std::string MakeUserAgentForSync(version_info::Channel channel) {
  return internal::FormatUserAgentForSync(GetSystemString(), channel);
}

}  // namespace syncer
