// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include <stdlib.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/environment.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

namespace chrome {

namespace {

struct ChannelState {
  version_info::Channel channel;
  bool is_extended_stable;
};

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
std::string GetChannelEnv() {
  auto env = base::Environment::Create();
  std::optional<std::string> channel_env =
      env->GetVar(version_info::nix::kChromeVersionExtra);
  return channel_env.value_or(std::string());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Returns the channel state for the browser based on branding and the
// CHROME_VERSION_EXTRA environment variable. In unbranded (Chromium) builds,
// this function unconditionally returns `channel` = UNKNOWN and
// `is_extended_stable` = false. In branded (Google Chrome) builds, this
// function returns `channel` = UNKNOWN and `is_extended_stable` = false for any
// unexpected $CHROME_VERSION_EXTRA value.
ChannelState GetChannelImpl() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto env = base::Environment::Create();
  return {version_info::nix::GetChannel(*env),
          version_info::nix::IsExtendedStable(*env)};
#else
  return {version_info::Channel::UNKNOWN, /*is_extended_stable=*/false};
#endif
}

}  // namespace

std::string GetChannelName(WithExtendedStable with_extended_stable) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto channel_state = GetChannelImpl();
  switch (channel_state.channel) {
    case version_info::Channel::UNKNOWN:
      return "unknown";
    case version_info::Channel::CANARY:
      return "canary";
    case version_info::Channel::DEV:
      return "dev";
    case version_info::Channel::BETA:
      return "beta";
    case version_info::Channel::STABLE:
      if (with_extended_stable && channel_state.is_extended_stable) {
        return "extended";
      }
      return std::string();
  }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return GetChannelEnv();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::string GetChannelSuffixForDataDir() {
  switch (GetChannel()) {
    case version_info::Channel::BETA:
      return "-beta";
    case version_info::Channel::DEV:
      return "-unstable";
    case version_info::Channel::CANARY:
      return "-canary";
    default:
      // Stable, extended stable, and unknown (e.g. in unbranded builds) don't
      // get a suffix.
      return std::string();
  }
}

#if BUILDFLAG(IS_LINUX)
std::string GetChannelSuffixForExtraFlagsEnvVarName() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const auto channel_state = GetChannelImpl();
  switch (channel_state.channel) {
    case version_info::Channel::CANARY:
      return "_CANARY";
    case version_info::Channel::DEV:
      return "_DEV";
    case version_info::Channel::BETA:
      return "_BETA";
    case version_info::Channel::STABLE:
      return "_STABLE";
    default:
      return std::string();
  }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string channel_name = GetChannelEnv();
  return channel_name.empty()
             ? std::string()
             : base::StrCat({"_", base::ToUpperASCII(channel_name)});
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
std::string GetDesktopName(base::Environment* env) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Google Chrome packaged as a snap is a special case: the application name
  // is always "google-chrome", regardless of the channel (channels are built
  // in to snapd, switching between them or doing parallel installs does not
  // require distinct application names).
  std::string snap_name = env->GetVar("SNAP_NAME").value_or(std::string());
  if (snap_name == "google-chrome") {
    return "google-chrome.desktop";
  }
  version_info::Channel product_channel(GetChannel());
  switch (product_channel) {
    case version_info::Channel::CANARY:
      return "google-chrome-canary.desktop";
    case version_info::Channel::DEV:
      return "google-chrome-unstable.desktop";
    case version_info::Channel::BETA:
      return "google-chrome-beta.desktop";
    default:
      // Extended stable is not differentiated from regular stable.
      return "google-chrome.desktop";
  }
#else  // BUILDFLAG(CHROMIUM_BRANDING)
  // Allow $CHROME_DESKTOP to override the built-in value, so that development
  // versions can set themselves as the default without interfering with
  // non-official, packaged versions using the built-in value.
  std::optional<std::string> name = env->GetVar("CHROME_DESKTOP");
  if (name.has_value() && !name.value().empty()) {
    return name.value();
  }
  return "chromium-browser.desktop";
#endif
}
#endif  // BUILDFLAG(IS_LINUX)

version_info::Channel GetChannel() {
  return GetChannelImpl().channel;
}

bool IsExtendedStableChannel() {
  return GetChannelImpl().is_extended_stable;
}

}  // namespace chrome
