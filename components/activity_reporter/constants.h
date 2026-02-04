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

inline constexpr std::string_view kChromeActivityId =
    "{afb5a10b-511f-4904-9596-d3f2937fd403}";

// Refer to ./buildflags.gni to enable/disable reporting.
inline constexpr std::string_view kUrl =
    "https://update.googleapis.com/service/update2/json";

}  // namespace activity_reporter

#endif  // BUILDFLAG(USE_ACTIVITY_REPORTER_INTERNAL_CONSTANTS)

#endif  // COMPONENTS_ACTIVITY_REPORTER_CONSTANTS_H_
