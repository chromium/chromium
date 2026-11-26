// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include <string>
#include <vector>

namespace base {
class Environment;
}
#endif

namespace security_interstitials {

// Provides utilities for security interstitials on //content-based platforms.

#if !BUILDFLAG(IS_CHROMEOS)
// Launches date and time settings as appropriate based on the platform (not
// supported on ChromeOS, where taking this action requires embedder-level
// machinery.
void LaunchDateAndTimeSettings();
#endif

#if BUILDFLAG(IS_LINUX)
// Returns the command line (program and arguments) that opens the date and
// time settings for the desktop environment described by `env`, or an empty
// vector if no suitable program is found in $PATH. Exposed for testing.
std::vector<std::string> GetDateAndTimeSettingsCommand(base::Environment* env);
#endif

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_
