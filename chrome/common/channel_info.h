// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHANNEL_INFO_H_
#define CHROME_COMMON_CHANNEL_INFO_H_

#include <string>

#include "base/types/strong_alias.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_LINUX)
namespace base {
class Environment;
}
#endif  // BUILDFLAG(IS_LINUX)

namespace version_info {
enum class Channel;
}

namespace chrome {

using WithExtendedStable = base::StrongAlias<class WithExtendedStableTag, bool>;

// Returns a version string containing the version number plus an optional
// channel identifier. For regular stable Chrome installs, this will be
// identical to version_info::GetVersionNumber(). For beta, dev, and canary
// installs, the version number is followed by the channel name. Extended stable
// installs appear identical to regular stable unless `with_extended_stable` is
// true.
// Prefer version_info::GetVersionNumber() if only the version number is needed.
std::string GetVersionString(WithExtendedStable with_extended_stable);

// Returns the name of the browser's update channel. For a branded build, this
// modifier is the channel ("canary", "dev", or "beta", but "" for stable).
//
// Ordinarily, extended stable is reported as "". Specify `with_extended_stable`
// to report extended stable as "extended". In general, this should be used for
// diagnostic strings in UX (e.g., in chrome://version). Whether or not it is
// used in strings sent to services (e.g., in the channel field of crash
// reports) is dependent on the specific service. `with_extended_stable` has no
// effect on Chrome OS Ash or Android due to lack of support for extended stable
// on those configurations.
//
// In branded builds, when the channel cannot be determined, "unknown" will be
// returned. In unbranded builds, the modifier is usually an empty string (""),
// although on Linux, it may vary in certain distributions. To simply test the
// channel, use GetChannel().
std::string GetChannelName(WithExtendedStable with_extended_stable);

// Returns the channel for the installation. In branded builds, this will be
// version_info::Channel::{STABLE,BETA,DEV,CANARY}. In unbranded builds, or
// in branded builds when the channel cannot be determined, this will be
// version_info::Channel::UNKNOWN.
version_info::Channel GetChannel();

// Returns true if this browser is on the extended stable channel. GetChannel()
// will always return version_info::Channel::STABLE when this is true. This
// function unconditionally returns false on Chrome OS Ash and Android due to
// lack of support for extended stable on those configurations.
bool IsExtendedStableChannel();

#if BUILDFLAG(IS_MAC)
// Because the channel information on the Mac is baked into the Info.plist file,
// and that file may change during an update, this function must be called
// early in startup to cache the channel info so that the correct channel info
// can be returned later.
void CacheChannelInfo();

// Maps the name of the channel to version_info::Channel, always returning
// Channel::UNKNOWN for unbranded builds. For branded builds defaults to
// Channel::STABLE, if channel is empty, else matches the name and returns
// {STABLE,BETA,DEV,CANARY, UNKNOWN}.
version_info::Channel GetChannelByName(const std::string& channel);

// Returns whether this is a side-by-side capable copy of Chromium. For
// unbranded builds, this is always true. For branded builds, this may not be
// true for old copies of beta and dev channels that share the same user data
// dir as the stable channel.
bool IsSideBySideCapable();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Sets/clears a KSChannelID value to be used in determining the browser's
// channel. The functions above will behave as if `channel_id` had been
// discovered as the channel identifier when CacheChannelInfo was called.
// Clearing reverts the change.
void SetChannelIdForTesting(const std::string& channel_id);
void ClearChannelIdForTesting();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns a channel-specific suffix to use when constructing the path of the
// default user data directory, allowing multiple channels to run side-by-side.
// In the stable channel and in unbranded builds, this returns the empty string.
std::string GetChannelSuffixForDataDir();
#endif

#if BUILDFLAG(IS_LINUX)
std::string GetChannelSuffixForExtraFlagsEnvVarName();

// Returns the channel-specific filename of the desktop shortcut used to launch
// the browser.
std::string GetDesktopName(base::Environment* env);
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace chrome

#endif  // CHROME_COMMON_CHANNEL_INFO_H_
