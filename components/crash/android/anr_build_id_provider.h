// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_ANDROID_ANR_BUILD_ID_PROVIDER_H_
#define COMPONENTS_CRASH_ANDROID_ANR_BUILD_ID_PROVIDER_H_

#include <string>

namespace crash_reporter {

// Returns the build ID for the elf file this has been compiled into.
std::string GetElfBuildId();

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_ANDROID_ANR_BUILD_ID_PROVIDER_H_
