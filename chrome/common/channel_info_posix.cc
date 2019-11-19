// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/environment.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

namespace chrome {

namespace {

// Helper function to return both the channel enum and modifier string.
// Implements both together to prevent their behavior from diverging, which has
// happened multiple times in the past.
version_info::Channel GetChannelImpl(std::string* modifier_out) {
  version_info::Channel channel = version_info::Channel::UNKNOWN;
  std::string modifier;

  char* env = getenv("CHROME_VERSION_EXTRA");
  if (env)
    modifier = env;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Only ever return "", "unknown", "dev" or "beta" in a branded build.
  if (modifier == "unstable")  // linux version of "dev"
    modifier = "dev";
  if (modifier == "stable") {
    channel = version_info::Channel::STABLE;
    modifier = "";
  } else if (modifier == "dev") {
    channel = version_info::Channel::DEV;
  } else if (modifier == "beta") {
    channel = version_info::Channel::BETA;
  } else {
    modifier = "unknown";
  }
#endif

  if (modifier_out)
    modifier_out->swap(modifier);

  return channel;
}

}  // namespace

std::string GetChannelName() {
  std::string modifier;
  GetChannelImpl(&modifier);
  return modifier;
}

std::string GetChannelSuffixForDataDir() {
  switch (GetChannel()) {
    case version_info::Channel::BETA:
      return "-beta";
    case version_info::Channel::DEV:
      return "-unstable";
    default:
      // Stable and unknown (e.g. in unbranded builds) don't get a suffix.
      return std::string();
  }
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
std::string GetDesktopName(base::Environment* env) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  version_info::Channel product_channel(GetChannel());
  switch (product_channel) {
    case version_info::Channel::DEV:
      return "google-chrome-unstable.desktop";
    case version_info::Channel::BETA:
      return "google-chrome-beta.desktop";
    default:
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
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

version_info::Channel GetChannel() {
  return GetChannelImpl(nullptr);
}

}  // namespace chrome
