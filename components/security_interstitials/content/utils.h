// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace security_interstitials {

// Provides utilities for security interstitials on //content-based platforms.

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Launches date and time settings as appropriate based on the platform (not
// supported on ChromeOS, where taking this action requires embedder-level
// machinery.
void LaunchDateAndTimeSettings();
#endif

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UTILS_H_
