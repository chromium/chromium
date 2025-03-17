// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace crosapi {

// The default user-data-directory for Lacros.
// NOTE: This is security sensitive. The directory must be inside the encrypted
// user data partition.
const char kLacrosUserDataPath[] = "/home/chronos/user/lacros";

// Release channel key in /etc/lsb-release.
const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";

// Release channel values in /etc/lsb-release.
const char kReleaseChannelCanary[] = "canary-channel";
const char kReleaseChannelDev[] = "dev-channel";
const char kReleaseChannelBeta[] = "beta-channel";
const char kReleaseChannelStable[] = "stable-channel";
const char kReleaseChannelLtc[] = "ltc-channel";
const char kReleaseChannelLts[] = "lts-channel";

}  // namespace crosapi
