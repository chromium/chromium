// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_BUILD_INFO_H_
#define CHROMECAST_CRASH_BUILD_INFO_H_

#include <string>

namespace chromecast {

// Returns a short, readable version string.
const std::string GetVersionString();

// Return whether build is "user", or "eng"
const std::string GetBuildVariant();

// Used by tests.
const std::string VersionToCrashString(const std::string& cast_build_revision);
const std::string VersionToVariant(const std::string& cast_build_revision);


}  // namespace chromecast

#endif  // CHROMECAST_CRASH_BUILD_INFO_H_
