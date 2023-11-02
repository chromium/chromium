// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_CLIENT_UPLOAD_INFO_H_
#define COMPONENTS_CRASH_CORE_APP_CLIENT_UPLOAD_INFO_H_

#include <string>

#include "build/build_config.h"

namespace crash_reporter {

// Returns whether the user has consented to collecting stats.
bool GetClientCollectStatsConsent();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
// Returns a textual description of the product type, version and channel
// to include in crash reports.
// TODO(https://crbug.com/986178): Implement this for other platforms.
void GetClientProductNameAndVersion(std::string* product,
                                    std::string* version,
                                    std::string* channel);
#endif

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_CLIENT_UPLOAD_INFO_H_
