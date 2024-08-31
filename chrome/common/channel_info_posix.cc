// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include <stdlib.h>

#include <string>
#include <string_view>

#include "base/environment.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/version_info/version_info.h"

namespace chrome {

namespace {

struct ChannelState {
  version_info::Channel channel;
  bool is_extended_stable;
};

// Returns the channel state for the browser based on branding and the
// CHROME_VERSION_EXTRA environment variable. In unbranded (Chromium) builds,
// this function unconditionally returns `channel` = UNKNOWN and
// `is_extended_stable` = false. In branded (Google Chrome) builds, this
// function returns `channel` = UNKNOWN and `is_extended_stable` = false for any
// unexpected $CHROME_VERSION_EXTRA value.
ChannelState GetChannelImpl() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char* const env = getenv("CHROME_VERSION_EXTRA");
  const std::string_view env_str =
      env ? std::string_view(env) : std::string_view();

  // Ordered by decreasing expected population size.
  if (env_str == "stable")
    return {version_info::Channel::STABLE, /*is_extended_stable=*/false};
  if (env_str == "extended")
    return {version_info::Channel::STABLE, /*is_extended_stable=*/true};
  if (env_str == "beta")
    return {version_info::Channel::BETA, /*is_extended_stable=*/false};
  if (env_str == "unstable")  // linux version of "dev"
    return {version_info::Channel::DEV, /*is_extended_stable=*/false};
  if (env_str == "canary") {
    return {version_info::Channel::CANARY, /*is_extended_stable=*/false};
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  return {version_info::Channel::UNKNOWN, /*is_extended_stable=*/false};
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
      if (with_extended_stable && channel_state.is_extended_stable)
        return "extended";
      return std::string();
  }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char* const env = getenv("CHROME_VERSION_EXTRA");
  return env ? std::string(std::string_view(env)) : std::string();
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
  const char* const channel_name = getenv("CHROME_VERSION_EXTRA");
  return channel_name
             ? base::StrCat(
                   {"_", base::ToUpperASCII(std::string_view(channel_name))})
             : std::string();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(IS_LINUX)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
std::string GetDesktopName(base::Environment* env) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Google Chrome packaged as a snap is a special case: the application name
  // is always "google-chrome", regardless of the channel (channels are built
  // in to snapd, switching between them or doing parallel installs does not
  // require distinct application names).
  std::string snap_name;
  if (env->GetVar("SNAP_NAME", &snap_name) && snap_name == "google-chrome")
    return "google-chrome.desktop";
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
  std::string name;
  if (env->GetVar("CHROME_DESKTOP", &name) && !name.empty())
    return name;
  return "chromium-browser.desktop";
#endif
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

version_info::Channel GetChannel() {
  return GetChannelImpl().channel;
}

bool IsExtendedStableChannel() {
  return GetChannelImpl().is_extended_stable;
}

}  // namespace chrome
