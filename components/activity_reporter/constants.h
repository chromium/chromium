// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_CONSTANTS_H_
#define COMPONENTS_ACTIVITY_REPORTER_CONSTANTS_H_

#include "components/update_client/configurator.h"

namespace activity_reporter {

inline constexpr char kChromeActivityId[] =
    "{afb5a10b-511f-4904-9596-d3f2937fd403}";

// Refer to ./buildflags.gni to enable/disable reporting.
inline constexpr char kUrl[] = "https://update.googleapis.com/service2/json";

}  // namespace activity_reporter

#endif  // COMPONENTS_ACTIVITY_REPORTER_CONSTANTS_H_
