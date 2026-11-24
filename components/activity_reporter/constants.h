// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_CONSTANTS_H_
#define COMPONENTS_ACTIVITY_REPORTER_CONSTANTS_H_

#include "components/activity_reporter/buildflags.h"

#if BUILDFLAG(USE_ACTIVITY_REPORTER_INTERNAL_CONSTANTS)

// Delegate to the internal constants header.
#include "components/activity_reporter/internal/constants.h"

#else

#include <string_view>

namespace activity_reporter {

// The app id that all Chrome use is reported under. On Android, that includes
// both browser and custom tab use.
inline constexpr std::string_view kChromeActivityId =
    "{afb5a10b-511f-4904-9596-d3f2937fd403}";

// The app id that custom tab use is reported under.
inline constexpr std::string_view kCustomTabActivityId =
    "{1679af61-6450-4560-8ce4-13f535c5fce9}";

// The app id that browser use, outside custom tabs, is reported under.
inline constexpr std::string_view kBrowserActivityId =
    "{495846a7-4e04-48d7-8d10-804961db3e79}";

// Refer to ./buildflags.gni to enable/disable reporting.
inline constexpr std::string_view kUrl =
    "https://update.googleapis.com/service/update2/json";

}  // namespace activity_reporter

#endif  // BUILDFLAG(USE_ACTIVITY_REPORTER_INTERNAL_CONSTANTS)

#endif  // COMPONENTS_ACTIVITY_REPORTER_CONSTANTS_H_
