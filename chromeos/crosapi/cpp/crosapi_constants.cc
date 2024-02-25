// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace crosapi {

// The prefix for a Wayland app id for a Lacros browser window. The full ID is
// suffixed with a serialized unguessable token unique to each window. The
// trailing "." is intentional.
const char kLacrosAppIdPrefix[] = "org.chromium.lacros.";

// The default Ash-side data directory.
const char kAshDataDir[] = "/home/chronos";

// The default user-data-directory for Lacros.
// NOTE: This is security sensitive. The directory must be inside the encrypted
// user data partition.
const char kLacrosUserDataPath[] = "/home/chronos/user/lacros";

// The default directory that contains Lacros files shared across users.
const char kLacrosSharedDataPath[] = "/home/chronos/lacros_shared";

// The file name of shared resource file.
const char kSharedResourcesPackName[] = "resources_for_sharing.rspak";
const char kSharedChrome100PercentPackName[] =
    "chrome_100_percent_for_sharing.rspak";
const char kSharedChrome200PercentPackName[] =
    "chrome_200_percent_for_sharing.rspak";

// Release channel key in /etc/lsb-release.
const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";

// Release channel values in /etc/lsb-release.
const char kReleaseChannelCanary[] = "canary-channel";
const char kReleaseChannelDev[] = "dev-channel";
const char kReleaseChannelBeta[] = "beta-channel";
const char kReleaseChannelStable[] = "stable-channel";
const char kReleaseChannelLtc[] = "ltc-channel";
const char kReleaseChannelLts[] = "lts-channel";

// Commandline flag whose value is the FD connected to Crosapi.
const char kCrosapiMojoPlatformChannelHandle[] =
    "crosapi-mojo-platform-channel-handle";

}  // namespace crosapi
