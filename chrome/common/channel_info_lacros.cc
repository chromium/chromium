// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include <stdlib.h>

#include <string>

#include "base/environment.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
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
  const base::StringPiece env_str =
      env ? base::StringPiece(env) : base::StringPiece();

  // Ordered by decreasing expected population size.
  if (env_str == "stable")
    return {version_info::Channel::STABLE, /*is_extended_stable=*/false};
  if (env_str == "extended")
    return {version_info::Channel::STABLE, /*is_extended_stable=*/true};
  if (env_str == "beta")
    return {version_info::Channel::BETA, /*is_extended_stable=*/false};
  if (env_str == "canary")
    return {version_info::Channel::CANARY, /*is_extended_stable=*/false};
  if (env_str == "dev")
    return {version_info::Channel::DEV, /*is_extended_stable=*/false};
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
  return env ? std::string(base::StringPiece(env)) : std::string();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

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
    case version_info::Channel::DEV:
      return "google-chrome-dev.desktop";
    case version_info::Channel::CANARY:
      return "google-chrome-canary.desktop";
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

version_info::Channel GetChannel() {
  return GetChannelImpl().channel;
}

bool IsExtendedStableChannel() {
  return GetChannelImpl().is_extended_stable;
}

}  // namespace chrome
