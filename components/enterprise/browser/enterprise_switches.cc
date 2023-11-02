// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/enterprise_switches.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace switches {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Enables the Chrome Browser Cloud Management integration on Chromium builds.
// CBCM is always enabled in branded builds.
const char kEnableChromeBrowserCloudManagement[] =
    "enable-chrome-browser-cloud-management";
#endif

}  // namespace switches
