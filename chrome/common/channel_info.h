// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHANNEL_INFO_H_
#define CHROME_COMMON_CHANNEL_INFO_H_

#include <string>

#include "build/build_config.h"

namespace base {
class Environment;
}

namespace version_info {
enum class Channel;
}

namespace chrome {

// Returns a version string to be displayed in "About Chromium" dialog.
std::string GetVersionString();

// Returns a human-readable modifier for the version string. For a branded
// build, this modifier is the channel ("canary", "dev", or "beta", but ""
// for stable). On Windows, this may be modified with additional information
// after a hyphen. For multi-user installations, it will return "canary-m",
// "dev-m", "beta-m", and for a stable channel multi-user installation, "m".
// In branded builds, when the channel cannot be determined, "unknown" will
// be returned. In unbranded builds, the modifier is usually an empty string
// (""), although on Linux, it may vary in certain distributions.
// GetChannelName() is intended to be used for display purposes.
// To simply test the channel, use GetChannel().
std::string GetChannelName();

// Returns the channel for the installation. In branded builds, this will be
// version_info::Channel::{STABLE,BETA,DEV,CANARY}. In unbranded builds, or
// in branded builds when the channel cannot be determined, this will be
// version_info::Channel::UNKNOWN.
version_info::Channel GetChannel();

#if defined(OS_MACOSX)
// Maps the name of the channel to version_info::Channel, always returning
// Channel::UNKNOWN for unbranded builds. For branded builds defaults to
// Channel::STABLE, if channel is empty, else matches the name and returns
// {STABLE,BETA,DEV,CANARY, UNKNOWN}.
version_info::Channel GetChannelByName(const std::string& channel);
#endif

#if defined(OS_POSIX)
// Returns a channel-specific suffix to use when constructing the path of the
// default user data directory, allowing multiple channels to run side-by-side.
// In the stable channel and in unbranded builds, this returns the empty string.
std::string GetChannelSuffixForDataDir();
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Returns the channel-specific filename of the desktop shortcut used to launch
// the browser.
std::string GetDesktopName(base::Environment* env);
#endif

}  // namespace chrome

#endif  // CHROME_COMMON_CHANNEL_INFO_H_
